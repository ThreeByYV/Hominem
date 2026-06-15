#include "hmnpch.h"
#include "Renderer3D.h"
#include "Hominem/Utils/Renderer.h"
#include "RenderCommand.h"
#include "Hominem/Core/Profiler.h"
#include "EnvironmentProbe.h"

#include <glad/glad.h>
#include <ranges>
#include <glm/gtc/matrix_inverse.hpp>

namespace Hominem {

namespace {

    struct alignas(16) GPULight
    {
        glm::vec4 positionAndRadius; // xyz = world pos, w = radius
        glm::vec4 colorAndIntensity; // xyz = colour,    w = intensity
        glm::vec4 directionAndType;  // xyz = direction (spot), w = type (0=point 1=spot)
        glm::vec4 coneAngles;        // x = cos(innerAngle), y = cos(outerAngle), z = sourceRadius, w = 0
    };

    struct LightGridEntry
    {
        uint32_t offset;
        uint32_t count;
    };

} 
    Renderer3DStorage*     Renderer3D::s_Data         = nullptr;
    bool                   Renderer3D::s_DrawNormals   = false;
    float                  Renderer3D::s_NormalLength  = 0.1f;
    bool                   Renderer3D::s_DrawAABB               = false;
    bool                   Renderer3D::s_DrawBoneWeights        = false;
    int                    Renderer3D::s_DisplayBoneIndex       = 0;
    bool                   Renderer3D::s_ToonShading            = false;
    bool                   Renderer3D::s_DebugHeatmap            = false;
    bool                   Renderer3D::s_AreaLightsEnabled       = true;
    float                  Renderer3D::s_RecommendedRenderScale  = 1.0f;
    uint32_t               Renderer3D::s_DrawCalls     = 0;
    uint64_t               Renderer3D::s_Triangles     = 0;
    uint32_t               Renderer3D::s_GroupsTotal   = 0;
    uint32_t               Renderer3D::s_GroupsCulled  = 0;

void Renderer3D::Init()
{
    s_Data  = new Renderer3DStorage();

    s_Data->ShaderLibrary = CreateRef<ShaderLibrary>();

    s_Data->ShaderLibrary->Load("engine://Shaders/fog.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/debug_aabb.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/debug_sphere.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/composite.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/bloom_threshold.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/bloom_blur.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/skybox.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/silhouette.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/fire_quad.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/smoke_quad.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/irradiance_convolve.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/prefilter_convolve.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/brdf_lut.glsl");

    s_Data->BRDFLUT = EnvironmentProbe::BakeBRDFLUT();

    s_Data->NormalsShader        = Shader::Create("engine://Shaders/normals_debug.glsl");
    s_Data->NormalsSkinnedShader = Shader::Create("engine://Shaders/normals_debug.glsl", {"SKINNED"});
    s_Data->BoneWeightShader     = Shader::Create("engine://Shaders/bone_weight.glsl");
    s_Data->DebugAABBShader      = s_Data->ShaderLibrary->Get("debug_aabb");
    s_Data->DebugSphereShader    = s_Data->ShaderLibrary->Get("debug_sphere");

    // StaticMeshShader is assigned by InitForwardPlus() on the render thread.

    static const uint32_t k_EdgeIndices[24] = {
        0,1, 1,2, 2,3, 3,0,
        4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
    };

    s_Data->DebugVBO = VertexBuffer::Create(8 * sizeof(glm::vec3));
    s_Data->DebugVBO->SetLayout({{ ShaderDataType::Float3, "a_Position" }});
    s_Data->DebugIBO = IndexBuffer::Create(const_cast<uint32_t*>(k_EdgeIndices), 24);
    s_Data->DebugVAO = VertexArray::Create();
    s_Data->DebugVAO->AddVertexBuffer(s_Data->DebugVBO);
    s_Data->DebugVAO->SetIndexBuffer(s_Data->DebugIBO);
}

static bool DetectLowEndGPU()
{
    const char* renderer = RenderCommand::GetGPURenderer();
    const char* vendor   = RenderCommand::GetGPUVendor();
    if (!renderer || !vendor) return false;
    std::string r(renderer), v(vendor);
    // Intel integrated (HD/UHD) — Arc is discrete and handles PBR fine
    if (v.find("Intel") != std::string::npos)
    {
        if (r.find("Arc") != std::string::npos) return false;
        if (r.find("HD ") != std::string::npos || r.find("UHD ") != std::string::npos ||
            r.find("HD Graphics") != std::string::npos || r.find("UHD Graphics") != std::string::npos)
            return true;
    }
    return false;
}

void Renderer3D::InitForwardPlus()
{
    HMN_CORE_ASSERT(s_Data, "Renderer3D::InitForwardPlus called before Init()");

    s_Data->SceneUBO           = UniformBuffer::Create(sizeof(SceneUBOData), 0);
    s_Data->LightBuffer        = StorageBuffer::Create(MAX_LIGHTS * sizeof(GPULight));
    s_Data->GlobalLightCounter = StorageBuffer::Create(sizeof(uint32_t));

    // LightIndexList and LightGrid are sized per-viewport in ResizeTileBuffers().

    s_Data->LightCullingShader = ComputeShader::Create("engine://Shaders/light_culling.comp");

    s_Data->MeshVariants = ShaderVariantSet::Create(
        "engine://Shaders/mesh.glsl",
        GetAllVariants());

    if (DetectLowEndGPU())
    {
        s_RecommendedRenderScale = 0.80f;
        HMN_CORE_INFO("Renderer3D: low-end integrated GPU detected — recommended render scale 0.80");
    }

    HMN_CORE_INFO("Renderer3D: Forward+ initialised — MAX_LIGHTS={}, TILE_SIZE={}px",
                  MAX_LIGHTS, TILE_SIZE);
}

void Renderer3D::Shutdown()
{
    delete s_Data;  s_Data  = nullptr;
}

void Renderer3D::ResizeTileBuffers(uint32_t w, uint32_t h)
{
    if (w == s_Data->ViewportW && h == s_Data->ViewportH) return;
    s_Data->ViewportW = w;
    s_Data->ViewportH = h;
    s_Data->NumTilesX = (w + TILE_SIZE - 1u) / TILE_SIZE;
    s_Data->NumTilesY = (h + TILE_SIZE - 1u) / TILE_SIZE;

    const uint32_t numTiles = s_Data->NumTilesX * s_Data->NumTilesY;
    s_Data->LightIndexList = StorageBuffer::Create(numTiles * MAX_LIGHTS_PER_TILE * sizeof(uint32_t));
    s_Data->LightGrid      = StorageBuffer::Create(numTiles * sizeof(LightGridEntry));
}

void Renderer3D::CullLights(const RenderFrame& frame, CommandList& cmd)
{
    HMN_PROFILE_FUNCTION();

    // Build GPU light list (pure CPU — safe on main thread)
    const uint32_t lightCount =
        static_cast<uint32_t>(std::min(frame.lights.size(), (size_t)MAX_LIGHTS));

    std::vector<GPULight> gpuLights;
    gpuLights.reserve(lightCount);
    std::ranges::transform(
        frame.lights | std::views::take(lightCount),
        std::back_inserter(gpuLights),
        [](const Light& l) -> GPULight {
            float cosInner = glm::cos(glm::radians(l.InnerAngle));
            float cosOuter = glm::cos(glm::radians(l.OuterAngle));
            return {
                { l.Position, l.Radius },
                { l.Color, l.Intensity },
                { l.Direction, static_cast<float>(l.Type) },
                { cosInner, cosOuter, l.SourceRadius, 0.f }
            };
        });

    // All SSBO/compute work runs on the render thread: ResizeTileBuffers calls
    // StorageBuffer::Create (glGenBuffers), which requires a valid GL context.
    cmd.Invoke([gpuLights = std::move(gpuLights), lightCount,
                viewW   = frame.viewportWidth,
                viewH   = frame.viewportHeight,
                view    = frame.view3D,
                invProj = glm::inverse(frame.proj3D)]() mutable
    {
        ResizeTileBuffers(viewW, viewH);

        constexpr uint32_t zero = 0u;
        s_Data->LightBuffer->SetData(gpuLights.data(), lightCount * sizeof(GPULight));
        s_Data->GlobalLightCounter->SetData(&zero, sizeof(uint32_t));

        s_Data->LightBuffer->BindBase(1);
        s_Data->LightIndexList->BindBase(2);
        s_Data->LightGrid->BindBase(3);
        s_Data->GlobalLightCounter->BindBase(4);

        const auto& cs = s_Data->LightCullingShader;
        cs->Bind();
        cs->SetMat4("u_View",          view);
        cs->SetMat4("u_InvProjection", invProj);
        cs->SetUint("u_NumLights",     lightCount);
        cs->SetUint("u_NumTilesX",     s_Data->NumTilesX);
        cs->SetUint("u_NumTilesY",     s_Data->NumTilesY);
        cs->SetUint("u_ScreenWidth",   viewW);
        cs->SetUint("u_ScreenHeight",  viewH);
        cs->Dispatch(s_Data->NumTilesX, s_Data->NumTilesY, 1);
        // Slots 1-3 remain bound for the subsequent shading pass.
    });
}

Renderer3D::SceneData Renderer3D::BeginScene(const RenderFrame& frame, CommandList& cmd)
{
    HMN_CORE_ASSERT(s_Data, "Renderer3D::BeginScene called before Init()");

    SceneData scene;
    const uint32_t envID    = frame.envMap ? frame.envMap->GetRendererID() : 0u;
    scene.EnvMapID          = envID;
    scene.IrradianceMapID   = frame.irradianceMap  ? frame.irradianceMap->GetRendererID()  : 0u;
    scene.PrefilteredMapID  = frame.prefilteredMap ? frame.prefilteredMap->GetRendererID() : 0u;
    scene.EnvMapIntensity   = (envID != 0u) ? frame.envMapIntensity : 0.f;
    scene.ETA               = frame.eta;
    scene.FresnelPower      = frame.fresnelPower;
    scene.Lights            = frame.lights;
    scene.CameraFrustum     = frame.frustum3D;
    s_DrawCalls              = 0;
    s_Triangles              = 0;
    s_GroupsTotal            = 0;
    s_GroupsCulled           = 0;

    // Upload all scene-wide data once — every shader reads it from binding 0
    if (s_Data->SceneUBO)
    {
        SceneUBOData ubo;
        ubo.ViewProjection   = frame.viewProjection3D;
        ubo.CameraWorldPos   = glm::vec4(frame.cameraWorldPos, 0.f);
        ubo.LightDirection   = glm::vec4(frame.light.Direction, 0.f);
        ubo.LightColor       = glm::vec4(frame.light.Color, 0.f);
        ubo.AmbientColor     = glm::vec4(frame.light.AmbientColor, 0.f);
        ubo.AmbientIntensity = frame.light.AmbientIntensity;
        ubo.DiffuseIntensity = frame.light.DiffuseIntensity;
        ubo.EnvMapIntensity  = scene.EnvMapIntensity;
        ubo.ETA              = scene.ETA;
        ubo.FresnelPower     = scene.FresnelPower;
        ubo.ScreenWidth      = frame.viewportWidth;
        ubo.DebugMode        = s_DebugHeatmap ? 1 : 0;
        ubo.AreaLightsEnabled = s_AreaLightsEnabled ? 1 : 0;
        cmd.SetUniformBufferData(s_Data->SceneUBO, &ubo, sizeof(ubo));
    }

    if (s_Data->LightCullingShader && !frame.lights.empty())
        CullLights(frame, cmd);

    return scene;
}


void Renderer3D::EndScene()
{
    Shader::UnbindAll();
    Texture::UnbindAll();
}

void Renderer3D::DrawSkinnedMesh(SkinnedMesh& mesh, const glm::mat4& transform, CommandList& cmd, const SceneData& scene)
{
    HMN_PROFILE_FUNCTION();
    const Material& mat0 = mesh.GetMaterial();

    if (s_DrawBoneWeights && s_Data->BoneWeightShader)
    {
        // Light culling overwrites bindings 3 and 4 — restore the mesh SSBOs before drawing.
        mesh.DispatchSkinning({}, cmd);
        cmd.BindShader(s_Data->BoneWeightShader);
        cmd.SetMat4 (s_Data->BoneWeightShader, "u_Model", transform);
        cmd.SetInt  (s_Data->BoneWeightShader, "u_Albedo", 0);
        cmd.SetFloat4(s_Data->BoneWeightShader, "u_Color", glm::vec4(1.f));
        cmd.SetInt  (s_Data->BoneWeightShader, "gDisplayBoneIndex", s_DisplayBoneIndex);
        mesh.Render(s_Data->BoneWeightShader, cmd);
        s_DrawCalls += mesh.GetSubmeshCount();
        s_Triangles += mesh.GetIndexCount() / 3;
        return;
    }

    Ref<Shader> shader = s_Data->OverrideShader;
    if (!shader)
    {
        HMN_CORE_ASSERT(s_Data->MeshVariants, "Renderer3D: variants not loaded");
        const bool hasNM = mat0.NormalMap      != nullptr;
        const bool hasMR = mat0.MetalRoughnessMap != nullptr;
        const bool toon  = s_ToonShading;
        std::string name = toon ? "skinned_toon" : "skinned_pbr";
        if (hasNM && hasMR) name += "_nm_mr";
        else if (hasNM)     name += "_nm";
        else if (hasMR)     name += "_mr";
        shader = s_Data->MeshVariants->Get(name);
    }
    HMN_CORE_ASSERT(shader, "Renderer3D: no shader for skinned mesh");

    cmd.BindShader(shader);
    cmd.SetInt (shader, "u_Albedo", 0);
    cmd.SetMat4(shader, "u_Model", transform);
    const Material& mat = mat0;
    cmd.SetFloat(shader, "u_Roughness", mat.Roughness);
    cmd.SetFloat(shader, "u_Metalness", mat.Metalness);

    const int lightCount = (int)std::min(scene.Lights.size(), (size_t)MAX_POINT_LIGHTS_SKINNED);
    cmd.SetInt(shader, "u_PointLightCount", lightCount);
    for (int i = 0; i < lightCount; i++)
    {
        const auto& l   = scene.Lights[i];
        const std::string idx = "[" + std::to_string(i) + "]";
        cmd.SetFloat3(shader, "u_PointLightPositions"  + idx, l.Position);
        cmd.SetFloat3(shader, "u_PointLightColors"     + idx, l.Color);
        cmd.SetFloat (shader, "u_PointLightIntensities"+ idx, l.Intensity);
        cmd.SetFloat (shader, "u_PointLightRadii"      + idx, l.Radius);
        cmd.SetFloat (shader, "u_PointLightSourceRadii"+ idx, l.SourceRadius);
    }

    mesh.Render(shader, cmd);
    s_DrawCalls += mesh.GetSubmeshCount();
    s_Triangles += mesh.GetIndexCount() / 3;

    if (s_DrawNormals && s_Data->NormalsSkinnedShader)
    {
        cmd.BindShader(s_Data->NormalsSkinnedShader);
        cmd.SetMat4 (s_Data->NormalsSkinnedShader, "u_Model", transform);
        cmd.SetFloat(s_Data->NormalsSkinnedShader, "u_NormalLength", s_NormalLength);
        mesh.Render(s_Data->NormalsSkinnedShader, cmd);
    }
}

void Renderer3D::DrawStaticMesh(StaticMesh& mesh, const glm::mat4& transform, CommandList& cmd, const SceneData& scene)
{
    HMN_PROFILE_FUNCTION();

    Ref<Shader> shader;
    if (s_Data->OverrideShader)
    {
        shader = s_Data->OverrideShader;
    }
    else
    {
        HMN_CORE_ASSERT(s_Data->MeshVariants, "Renderer3D: variants not loaded — was InitForwardPlus() called?");

        const bool hasNM  = mesh.HasNormalMap();
        const bool hasMR  = mesh.HasMetalRoughness();
        const bool hasEnv = scene.EnvMapID != 0u;
        const bool toon   = s_ToonShading;
        std::string name  = toon ? "static_toon" : "static_pbr";
        if (hasNM && hasMR && hasEnv) name += "_nm_mr_env";
        else if (hasNM && hasMR)      name += "_nm_mr";
        else if (hasMR && hasEnv)     name += "_mr_env";
        else if (hasNM && hasEnv)     name += "_nm_env";
        else if (hasNM)               name += "_nm";
        else if (hasMR)               name += "_mr";
        else if (hasEnv)              name += "_env";
        shader = s_Data->MeshVariants->Get(name);
    }
    HMN_CORE_ASSERT(shader, "Renderer3D::DrawStaticMesh: no shader variant available");

    cmd.BindShader(shader);

    if (scene.EnvMapID != 0u)
    {
        cmd.BindTexture(3, scene.EnvMapID);
        if (scene.IrradianceMapID != 0u)
        {
            cmd.BindTexture(4, scene.IrradianceMapID);
            cmd.SetInt(shader, "u_IrradianceMap", 4);
        }
        if (scene.PrefilteredMapID != 0u)
        {
            cmd.BindTexture(5, scene.PrefilteredMapID);
            cmd.SetInt(shader, "u_PrefilteredMap", 5);
        }
        cmd.BindTexture(6, s_Data->BRDFLUT->GetRendererID());
        cmd.SetInt(shader, "u_BRDFLUT", 6);
    }

    const Material& mat = mesh.GetMaterial();
    cmd.SetFloat(shader, "u_Roughness", mat.Roughness);
    cmd.SetFloat(shader, "u_Metalness", mat.Metalness);

    auto [calls, tris] = mesh.Draw(shader, transform, cmd, &scene.CameraFrustum);
    s_DrawCalls   += calls;
    s_Triangles   += tris;
    uint32_t total = static_cast<uint32_t>(mesh.GetDrawGroupCount());
    s_GroupsTotal  += total;
    s_GroupsCulled += total - calls;

    if (s_DrawNormals && s_Data->NormalsShader)
    {
        cmd.BindShader(s_Data->NormalsShader);
        cmd.SetFloat(s_Data->NormalsShader, "u_NormalLength", s_NormalLength);
        mesh.Draw(s_Data->NormalsShader, transform, cmd);
    }

    if (s_DrawAABB && s_Data->DebugAABBShader)
    {
        s_Data->DebugVAO->Bind();
        auto aabbCmd = RenderCommand::SetPipelineState(PipelineState::AlphaBlendNoDepth());
        aabbCmd.BindShader(s_Data->DebugAABBShader);

        const size_t groupCount = mesh.GetDrawGroupCount();
        for (size_t gi = 0; gi < groupCount; gi++)
        {
            auto [mn, mx]         = mesh.GetDrawGroupBounds(gi);
            const glm::mat4 model = transform * mesh.GetDrawGroupTransform(gi);
            const bool    visible = scene.CameraFrustum.TestAABBTransformed(mn, mx, model);

            const glm::vec3 corners[8] = {
                glm::vec3(model * glm::vec4(mn.x, mn.y, mn.z, 1.f)),
                glm::vec3(model * glm::vec4(mx.x, mn.y, mn.z, 1.f)),
                glm::vec3(model * glm::vec4(mx.x, mx.y, mn.z, 1.f)),
                glm::vec3(model * glm::vec4(mn.x, mx.y, mn.z, 1.f)),
                glm::vec3(model * glm::vec4(mn.x, mn.y, mx.z, 1.f)),
                glm::vec3(model * glm::vec4(mx.x, mn.y, mx.z, 1.f)),
                glm::vec3(model * glm::vec4(mx.x, mx.y, mx.z, 1.f)),
                glm::vec3(model * glm::vec4(mn.x, mx.y, mx.z, 1.f)),
            };
            aabbCmd.SetBufferData(s_Data->DebugVBO, corners, sizeof(corners));
            aabbCmd.SetFloat4(s_Data->DebugAABBShader, "u_Color",
                visible ? glm::vec4(0.f, 1.f, 0.f, 1.f)
                        : glm::vec4(1.f, 0.f, 0.f, 1.f));
            aabbCmd.DrawIndexedLines(s_Data->DebugVAO, 24);
        }
        aabbCmd.Submit();

        s_Data->DebugVAO->Unbind();
    }
}

void Renderer3D::DrawDebugLights(const std::vector<Light>& lights)
{
    if (lights.empty() || !s_Data->DebugSphereShader) return;

    s_Data->DebugVAO->Bind();
    auto cmd = RenderCommand::SetPipelineState(PipelineState::AlphaBlendNoDepth());
    cmd.BindShader(s_Data->DebugSphereShader);
    cmd.SetFloat(s_Data->DebugSphereShader, "u_TessLevel", 16.f);

    for (const auto& light : lights)
    {
        glm::vec3 pos = light.Position;
        float gizmoRadius = glm::clamp(light.Radius * 0.08f, 0.05f, 0.4f);
        cmd.SetBufferData(s_Data->DebugVBO, &pos, sizeof(glm::vec3));
        cmd.SetFloat4(s_Data->DebugSphereShader, "u_Color",  glm::vec4(light.Color, 1.f));
        cmd.SetFloat (s_Data->DebugSphereShader, "u_Radius", gizmoRadius);
        cmd.DrawPatches(1, 1);
    }
    cmd.Submit();
    s_Data->DebugVAO->Unbind();
}

}
