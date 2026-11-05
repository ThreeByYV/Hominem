#include "hmnpch.h"
#include "Hominem/Renderer/ClusteredRenderer.h"
#include "Hominem/Renderer/Frame/RenderSettings.h"
#include "Hominem/Renderer/RHI/RenderCommand.h"
#include "Hominem/Core/Profiler.h"
#include "Hominem/Renderer/Lighting/EnvironmentProbe.h"
#include "Hominem/Renderer/Frame/RenderThread.h"

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

    // std430: 2 vec4 + uint + uint[100] = 436 bytes, rounded up to the vec4 alignment
    struct alignas(16) GPUCluster
    {
        glm::vec4 minPoint;
        glm::vec4 maxPoint;
        uint32_t  count;
        uint32_t  lightIndices[ClusteredRenderer::MAX_LIGHTS_PER_CLUSTER];
    };
    static_assert(sizeof(GPUCluster) == 448, "GPUCluster must match the std430 Cluster stride");

    // Undoes glm::perspective (right handed, -1..1 depth) so callers don't have to pass near / far
    void ExtractNearFar(const glm::mat4& proj, float& zNear, float& zFar)
    {
        zNear = proj[3][2] / (proj[2][2] - 1.f);
        zFar  = proj[3][2] / (proj[2][2] + 1.f);
    }

}
    ClusteredRendererStorage*     ClusteredRenderer::s_Data         = nullptr;
    uint32_t                      ClusteredRenderer::s_DrawCalls     = 0;
    uint64_t                      ClusteredRenderer::s_Triangles     = 0;
    uint32_t                      ClusteredRenderer::s_GroupsTotal   = 0;
    uint32_t                      ClusteredRenderer::s_GroupsCulled  = 0;

void ClusteredRenderer::Init()
{
    s_Data  = new ClusteredRendererStorage();

    s_Data->NormalsShader        = Shader::Create("engine://Shaders/normals_debug.glsl");
    s_Data->NormalsSkinnedShader = Shader::Create("engine://Shaders/normals_debug.glsl", {"SKINNED"});
    s_Data->BoneWeightShader     = Shader::Create("engine://Shaders/bone_weight.glsl");
    s_Data->DebugAABBShader      = ShaderLibrary::Engine()->Load("engine://Shaders/debug_aabb.glsl");
    s_Data->DebugSphereShader    = ShaderLibrary::Engine()->Load("engine://Shaders/debug_sphere.glsl");
    // Games pick this up by name as a mesh override, from threads with no GL context.
    ShaderLibrary::Engine()->Load("engine://Shaders/silhouette.glsl");

    // StaticMeshShader is assigned by InitClustered() on the render thread.

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

void ClusteredRenderer::InitClustered()
{
    HMN_CORE_ASSERT(s_Data, "ClusteredRenderer::InitClustered called before Init()");

    s_Data->SceneUBO      = UniformBuffer::Create(sizeof(SceneUBOData), 0);
    s_Data->ClusterBuffer = StorageBuffer::Create(NUM_CLUSTERS * sizeof(GPUCluster));
    s_Data->LightBuffer   = StorageBuffer::Create(MAX_LIGHTS * sizeof(GPULight));

    s_Data->ClusterBuildShader = ComputeShader::Create("engine://Shaders/cluster_build.comp");
    s_Data->ClusterCullShader  = ComputeShader::Create("engine://Shaders/cluster_cull.comp");

    s_Data->MeshVariants = ShaderVariantSet::Create(
        "engine://Shaders/mesh.glsl",
        GetAllVariants());

    HMN_CORE_INFO("ClusteredRenderer: initialised, grid {}x{}x{} ({} clusters), MAX_LIGHTS={}",
                  GRID_SIZE_X, GRID_SIZE_Y, GRID_SIZE_Z, NUM_CLUSTERS, MAX_LIGHTS);
}

void ClusteredRenderer::Shutdown()
{
    delete s_Data;  s_Data  = nullptr;
}

void ClusteredRenderer::BuildClusters(const glm::mat4& invProj, uint32_t w, uint32_t h, float zNear, float zFar)
{
    if (w == s_Data->GridW && h == s_Data->GridH && invProj == s_Data->GridInvProjection) return;
    s_Data->GridW             = w;
    s_Data->GridH             = h;
    s_Data->GridInvProjection = invProj;

    constexpr uint32_t LOCAL_SIZE_XY = 8u;
    const auto& cs = s_Data->ClusterBuildShader;
    cs->Bind();
    cs->SetFloat ("zNear",             zNear);
    cs->SetFloat ("zFar",              zFar);
    cs->SetMat4  ("inverseProjection", invProj);
    cs->SetUint3 ("gridSize",          { GRID_SIZE_X, GRID_SIZE_Y, GRID_SIZE_Z });
    cs->SetFloat2("screenDimensions",  { (float)w, (float)h });
    cs->Dispatch((GRID_SIZE_X + LOCAL_SIZE_XY - 1u) / LOCAL_SIZE_XY,
                 (GRID_SIZE_Y + LOCAL_SIZE_XY - 1u) / LOCAL_SIZE_XY,
                 GRID_SIZE_Z);
}

void ClusteredRenderer::CullLights(const RenderFrame& frame, CommandList& cmd,
                                   uint32_t renderW, uint32_t renderH, float zNear, float zFar)
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

    // All SSBO/compute work runs on the render thread, it needs a valid GL context.
    // Unjittered so TAA's per-frame sub-pixel offset doesn't force a grid rebuild every frame.
    cmd.Invoke([gpuLights = std::move(gpuLights), lightCount,
                renderW, renderH, zNear, zFar,
                view    = frame.view3D,
                invProj = glm::inverse(frame.proj3DUnjittered)]() mutable
    {
        s_Data->LightBuffer->BindBase(1);
        s_Data->ClusterBuffer->BindBase(2);

        BuildClusters(invProj, renderW, renderH, zNear, zFar);

        s_Data->LightBuffer->SetData(gpuLights.data(), lightCount * sizeof(GPULight));

        constexpr uint32_t LOCAL_SIZE = 128u;
        const auto& cs = s_Data->ClusterCullShader;
        cs->Bind();
        cs->SetMat4("viewMatrix", view);
        cs->SetUint("lightCount", lightCount);
        cs->Dispatch((NUM_CLUSTERS + LOCAL_SIZE - 1u) / LOCAL_SIZE);
        // Slots 1-2 remain bound for the subsequent shading pass.
    });
}

ClusteredRenderer::SceneData ClusteredRenderer::BeginScene(const RenderFrame& frame, CommandList& cmd,
                                                           uint32_t renderW, uint32_t renderH)
{
    HMN_CORE_ASSERT(s_Data, "ClusteredRenderer::BeginScene called before Init()");

    // Reset all texture slots to 0 so every draw call in this scene must explicitly
    // bind what it needs. Prevents stale bindings.
    for (uint32_t i = 0; i < 16; i++)
        cmd.BindTexture(i, 0);

    // Use provided render dimensions; fall back to the frame's viewport if not given.
    // renderW/renderH must equal the actual HDR FBO size — when renderScale != 1.0 they
    // differ from frame.viewportWidth/Height (which is the window size), and using the
    // wrong value shifts every cluster tile index in the fragment shader.
    const uint32_t useW = (renderW != 0) ? renderW : frame.viewportWidth;
    const uint32_t useH = (renderH != 0) ? renderH : frame.viewportHeight;

    float zNear = 0.f, zFar = 0.f;
    ExtractNearFar(frame.proj3D, zNear, zFar);

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
        ubo.ScreenWidth      = useW;
        ubo.DebugMode         = RenderSettings::DebugHeatmap ? 1 : 0;
        ubo.AreaLightsEnabled = RenderSettings::AreaLights   ? 1 : 0;
        // Velocity is the difference between these two, so both must be jitter-free —
        // otherwise the per-frame sample offset reads as motion and cancels the accumulation.
        ubo.ViewProjectionUnjittered = frame.viewProjection3DUnjittered;
        ubo.PrevViewProjection       = frame.prevViewProjection3D;
        ubo.TextureLodBias           = frame.textureLodBias;
        ubo.ZNear                    = zNear;
        ubo.ZFar                     = zFar;
        ubo.ScreenHeight             = useH;
        ubo.View                     = frame.view3D;
        cmd.SetUniformBufferData(s_Data->SceneUBO, &ubo, sizeof(ubo));
    }

    // Always cull even with 0 lights, so every cluster count is reset. Skipping leaves
    // stale lists from a prior bake (512x512, other projection) or layer.
    if (s_Data->ClusterBuildShader && s_Data->ClusterCullShader)
        CullLights(frame, cmd, useW, useH, zNear, zFar);

    return scene;
}


void ClusteredRenderer::EndScene()
{
    Shader::UnbindAll();
    Texture::UnbindAll();
}

// Ray-traced indirect diffuse. Slots 7/8 stay bound to whatever was there when the volume
// is off — u_DDGICounts.w == 0 keeps the shader from sampling them.
static void BindDDGI(CommandList& cmd, const Ref<Shader>& shader, const ClusteredRenderer::SceneData& scene)
{
    const bool active = scene.DDGI.probeNumRays > 0
                     && scene.DDGIIrradianceID != 0
                     && scene.DDGIDistanceID   != 0;

    static bool s_LoggedState = false;
    if (!s_LoggedState && scene.DDGI.probeNumRays > 0)
    {
        s_LoggedState = true;
        HMN_CORE_INFO("DDGI (GL): volume active, irradiance tex {0}, distance tex {1} -> {2}",
                      scene.DDGIIrradianceID, scene.DDGIDistanceID,
                      active ? "sampling" : "NOT sampling (atlases missing)");
    }

    cmd.SetInt(shader, "u_DDGIDebug", RenderSettings::DDGIDebug ? 1 : 0);

    if (!active)
    {
        cmd.SetFloat4(shader, "u_DDGICounts", glm::vec4(0.f));
        return;
    }

    cmd.BindTexture(7, scene.DDGIIrradianceID);
    cmd.BindTexture(8, scene.DDGIDistanceID);
    cmd.SetInt(shader, "u_DDGIIrradiance", 7);
    cmd.SetInt(shader, "u_DDGIDistance",   8);

    cmd.SetFloat4(shader, "u_DDGIOrigin",  glm::vec4(scene.DDGI.origin, 0.f));
    cmd.SetFloat4(shader, "u_DDGISpacing", glm::vec4(scene.DDGI.probeSpacing, 0.f));
    cmd.SetFloat4(shader, "u_DDGICounts",  glm::vec4(scene.DDGI.probeCounts, (float)scene.DDGI.probeNumRays));
    cmd.SetFloat4(shader, "u_DDGITiles",   glm::vec4((float)kDDGIIrradianceInterior,
                                                     (float)kDDGIDistanceInterior, 0.f, 0.f));
    cmd.SetFloat(shader, "u_DDGIIntensity", scene.DDGI.intensity);
}

void ClusteredRenderer::DrawSkinnedMesh(SkinnedMesh& mesh, const glm::mat4& transform, CommandList& cmd, const SceneData& scene,
                                  const glm::mat4* prevTransform)
{
    HMN_PROFILE_FUNCTION();
    if (RenderSettings::DrawBoneWeights && s_Data->BoneWeightShader)
    {
        // Debug shader writes no velocity — mask the attachment so it keeps whatever the
        // scene put there rather than undefined values.
        cmd.SetColorMask(1, false);
        // Light culling overwrites bindings 1 and 2 — restore the mesh SSBOs before drawing.
        mesh.DispatchSkinning({}, cmd);
        cmd.BindShader(s_Data->BoneWeightShader);
        cmd.SetMat4 (s_Data->BoneWeightShader, "u_Model", transform);
        cmd.SetInt  (s_Data->BoneWeightShader, "u_Albedo", 0);
        cmd.SetFloat4(s_Data->BoneWeightShader, "u_Color", glm::vec4(1.f));
        cmd.SetInt  (s_Data->BoneWeightShader, "gDisplayBoneIndex", RenderSettings::DisplayBoneIndex);
        mesh.Render(s_Data->BoneWeightShader, cmd);
        cmd.SetColorMask(1, true);
        s_DrawCalls += mesh.GetSubmeshCount();
        s_Triangles += mesh.GetIndexCount() / 3;
        return;
    }

    Ref<Shader> shader = s_Data->OverrideShader;
    if (!shader)
    {
        HMN_CORE_ASSERT(s_Data->MeshVariants, "ClusteredRenderer: variants not loaded");
        const bool hasNM = mesh.HasNormalMap() && RenderSettings::SkinnedNormalMaps;
        const bool hasMR = mesh.HasMetalRoughness();
        const bool toon  = RenderSettings::ToonShading;
        std::string name = toon ? "skinned_toon" : "skinned_pbr";
        if (hasNM && hasMR) name += "_nm_mr";
        else if (hasNM)     name += "_nm";
        else if (hasMR)     name += "_mr";
        shader = s_Data->MeshVariants->Get(name);
    }
    HMN_CORE_ASSERT(shader, "ClusteredRenderer: no shader for skinned mesh");

    cmd.BindShader(shader);
    cmd.SetInt (shader, "u_Albedo", 0);
    cmd.SetMat4(shader, "u_Model", transform);
    cmd.SetMat4(shader, "u_PrevM", prevTransform ? *prevTransform : transform);
    BindDDGI(cmd, shader, scene);

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

    if (RenderSettings::DrawNormals && s_Data->NormalsSkinnedShader)
    {
        cmd.SetColorMask(1, false); // no velocity output on the debug shader
        cmd.BindShader(s_Data->NormalsSkinnedShader);
        cmd.SetMat4 (s_Data->NormalsSkinnedShader, "u_Model", transform);
        cmd.SetFloat(s_Data->NormalsSkinnedShader, "u_NormalLength", RenderSettings::NormalLength);
        mesh.Render(s_Data->NormalsSkinnedShader, cmd);
        cmd.SetColorMask(1, true);
    }
}

void ClusteredRenderer::DrawStaticMesh(StaticMesh& mesh, const glm::mat4& transform, CommandList& cmd, const SceneData& scene,
                                 const glm::mat4* prevTransform)
{
    HMN_PROFILE_FUNCTION();

    Ref<Shader> shader;
    if (s_Data->OverrideShader)
    {
        shader = s_Data->OverrideShader;
    }
    else
    {
        HMN_CORE_ASSERT(s_Data->MeshVariants, "ClusteredRenderer: variants not loaded — was InitClustered() called?");

        const bool hasNM  = mesh.HasNormalMap();
        const bool hasMR  = mesh.HasMetalRoughness();
        const bool hasEnv = scene.EnvMapID != 0u;
        const bool toon   = RenderSettings::ToonShading;
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
    HMN_CORE_ASSERT(shader, "ClusteredRenderer::DrawStaticMesh: no shader variant available");

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
        cmd.BindTexture(6, EnvironmentProbe::GetBRDFLUT()->GetRendererID());
        cmd.SetInt(shader, "u_BRDFLUT", 6);
    }

    BindDDGI(cmd, shader, scene);


    auto [calls, tris] = mesh.Draw(shader, transform, cmd, &scene.CameraFrustum, prevTransform);
    s_DrawCalls   += calls;
    s_Triangles   += tris;
    uint32_t total = static_cast<uint32_t>(mesh.GetDrawGroupCount());
    s_GroupsTotal  += total;
    s_GroupsCulled += total - calls;

    if (RenderSettings::DrawNormals && s_Data->NormalsShader)
    {
        cmd.SetColorMask(1, false); // no velocity output on the debug shader
        cmd.BindShader(s_Data->NormalsShader);
        cmd.SetFloat(s_Data->NormalsShader, "u_NormalLength", RenderSettings::NormalLength);
        mesh.Draw(s_Data->NormalsShader, transform, cmd);
        cmd.SetColorMask(1, true);
    }

    if (RenderSettings::DrawAABB && s_Data->DebugAABBShader)
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

void ClusteredRenderer::DrawDebugLights(const std::vector<Light>& lights)
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
