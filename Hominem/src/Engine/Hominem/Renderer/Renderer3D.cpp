#include "hmnpch.h"
#include "Renderer3D.h"
#include "Hominem/Utils/Renderer.h"
#include "RenderCommand.h"

#include <ranges>
#include <glm/gtc/matrix_inverse.hpp>

namespace Hominem {

namespace {

    struct alignas(16) GPULight
    {
        glm::vec4 positionAndRadius; // xyz = world pos, w = radius
        glm::vec4 colorAndIntensity; // xyz = colour,    w = intensity
        glm::vec4 directionAndType;  // xyz = direction (spot), w = type (0=point 1=spot)
        glm::vec4 coneAngles;        // x = cos(innerAngle), y = cos(outerAngle), zw = 0
    };

    struct LightGridEntry
    {
        uint32_t offset;
        uint32_t count;
    };

} 
    Renderer3DStorage*     Renderer3D::s_Data         = nullptr;
    Renderer3D::SceneData* Renderer3D::s_Scene        = nullptr;
    bool                   Renderer3D::s_DrawNormals  = false;
    float                  Renderer3D::s_NormalLength = 0.1f;
    bool                   Renderer3D::s_DrawAABB     = false;
    bool                   Renderer3D::s_DebugHeatmap = false;
    uint32_t               Renderer3D::s_DrawCalls    = 0;
    uint64_t               Renderer3D::s_Triangles    = 0;

void Renderer3D::Init()
{
    s_Data  = new Renderer3DStorage();
    s_Scene = new SceneData();

    s_Data->ShaderLibrary = CreateRef<ShaderLibrary>();

    s_Data->ShaderLibrary->Load("engine://Shaders/fog.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/debug_aabb.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/debug_sphere.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/composite.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/bloom_threshold.glsl");
    s_Data->ShaderLibrary->Load("engine://Shaders/bloom_blur.glsl");

    s_Data->NormalsShader        = Shader::Create("engine://Shaders/normals_debug.glsl");
    s_Data->NormalsSkinnedShader = Shader::Create("engine://Shaders/normals_debug.glsl", {"SKINNED"});
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

void Renderer3D::InitForwardPlus()
{
    HMN_CORE_ASSERT(s_Data, "Renderer3D::InitForwardPlus called before Init()");

    s_Data->SceneUBO           = UniformBuffer::Create(sizeof(SceneUBOData), 0);
    s_Data->LightBuffer        = StorageBuffer::Create(MAX_LIGHTS * sizeof(GPULight));
    s_Data->GlobalLightCounter = StorageBuffer::Create(sizeof(uint32_t));

    // LightIndexList and LightGrid are sized per-viewport in ResizeTileBuffers().

    s_Data->LightCullingShader = ComputeShader::Create("engine://Shaders/light_culling.comp");

    s_Data->MeshVariants = ShaderPermutationSet::Create(
        "engine://Shaders/mesh.glsl",
        k_MeshPerms, std::size(k_MeshPerms));

    HMN_CORE_INFO("Renderer3D: Forward+ initialised — MAX_LIGHTS={}, TILE_SIZE={}px",
                  MAX_LIGHTS, TILE_SIZE);
}

void Renderer3D::Shutdown()
{
    delete s_Scene; s_Scene = nullptr;
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

void Renderer3D::CullLights(const RenderFrame& frame)
{
    ResizeTileBuffers(frame.viewportWidth, frame.viewportHeight);

    // Build GPU light list from RenderFrame
    const uint32_t lightCount =
        static_cast<uint32_t>(std::min(frame.lights.size(), (size_t)MAX_LIGHTS));

    // Pack CPU Light structs into GPU-ready GPULight vec4s for the SSBO.
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
                { cosInner, cosOuter, 0.f, 0.f }
            };
        });

    s_Data->LightBuffer->SetData(gpuLights.data(), lightCount * sizeof(GPULight));

    const uint32_t zero = 0u;
    s_Data->GlobalLightCounter->SetData(&zero, sizeof(uint32_t));

    s_Data->LightBuffer->BindBase(1);
    s_Data->LightIndexList->BindBase(2);
    s_Data->LightGrid->BindBase(3);
    s_Data->GlobalLightCounter->BindBase(4);

    s_Data->LightCullingShader->Bind();
    s_Data->LightCullingShader->SetMat4("u_View",          frame.view3D);
    s_Data->LightCullingShader->SetMat4("u_InvProjection", glm::inverse(frame.proj3D));
    s_Data->LightCullingShader->SetUint("u_NumLights",     lightCount);
    s_Data->LightCullingShader->SetUint("u_NumTilesX",     s_Data->NumTilesX);
    s_Data->LightCullingShader->SetUint("u_NumTilesY",     s_Data->NumTilesY);
    s_Data->LightCullingShader->SetUint("u_ScreenWidth",   frame.viewportWidth);
    s_Data->LightCullingShader->SetUint("u_ScreenHeight",  frame.viewportHeight);

    // Dispatch inserts GL_SHADER_STORAGE_BARRIER_BIT — SSBOs safe to read in fragment stage.
    s_Data->LightCullingShader->Dispatch(s_Data->NumTilesX, s_Data->NumTilesY, 1);

    // Slots 1-3 remain bound for the subsequent shading pass.
}

void Renderer3D::BeginScene(const RenderFrame& frame)
{
    HMN_CORE_ASSERT(s_Scene, "Renderer3D::BeginScene called before Init()");

    const uint32_t envID     = frame.envMap ? frame.envMap->GetRendererID() : 0u;
    s_Scene->EnvMapID        = envID;
    s_Scene->EnvMapIntensity = (envID != 0u) ? frame.envMapIntensity : 0.f;
    s_Scene->ETA             = frame.eta;
    s_Scene->FresnelPower    = frame.fresnelPower;
    s_Scene->Lights          = frame.lights;
    s_DrawCalls              = 0;
    s_Triangles              = 0;

    // Upload all scene-wide data once — every shader reads it from binding 0
    if (s_Data->SceneUBO)
    {
        SceneUBOData ubo;
        ubo.ViewProjection   = frame.viewProjection3D;
        ubo.View             = frame.view3D;
        ubo.InvProjection    = glm::inverse(frame.proj3D);
        ubo.CameraWorldPos   = glm::vec4(frame.cameraWorldPos, 0.f);
        ubo.LightDirection   = glm::vec4(frame.light.Direction, 0.f);
        ubo.LightColor       = glm::vec4(frame.light.Color, 0.f);
        ubo.AmbientColor     = glm::vec4(frame.light.AmbientColor, 0.f);
        ubo.AmbientIntensity = frame.light.AmbientIntensity;
        ubo.DiffuseIntensity = frame.light.DiffuseIntensity;
        ubo.EnvMapIntensity  = s_Scene->EnvMapIntensity;
        ubo.ETA              = s_Scene->ETA;
        ubo.FresnelPower     = s_Scene->FresnelPower;
        ubo.ScreenWidth      = frame.viewportWidth;
        ubo.DebugMode        = s_DebugHeatmap ? 1 : 0;
        ubo._pad             = 0.f;
        s_Data->SceneUBO->SetData(&ubo, sizeof(ubo));
    }

    if (s_Data->LightCullingShader && !frame.lights.empty())
        CullLights(frame);
}

void Renderer3D::EndScene()
{
    Shader::UnbindAll();
    Texture::UnbindAll();
}

void Renderer3D::DrawSkinnedMesh(SkinnedMesh& mesh, const glm::mat4& transform)
{
    Ref<Shader> shader = s_Data->OverrideShader;
    if (!shader)
    {
        HMN_CORE_ASSERT(s_Data->MeshVariants, "Renderer3D: variants not loaded");
        uint32_t perm = ResolvePermutation(ShaderPerm_Skinned, k_MeshPerms, std::size(k_MeshPerms));
        shader = s_Data->MeshVariants->GetVariant(perm);
    }
    HMN_CORE_ASSERT(shader, "Renderer3D: no shader for skinned mesh");

    shader->Bind();
    shader->SetInt("u_Albedo", 0);
    shader->SetMat4("u_Model", transform);
    const Material& mat = mesh.GetMaterial();
    shader->SetFloat("u_Roughness", mat.Roughness);
    shader->SetFloat("u_Metalness", mat.Metalness);

    const int lightCount = (int)std::min(s_Scene->Lights.size(), (size_t)MAX_POINT_LIGHTS_SKINNED);
    shader->SetInt("u_PointLightCount", lightCount);
    for (int i = 0; i < lightCount; i++)
    {
        const auto& l   = s_Scene->Lights[i];
        const std::string idx = "[" + std::to_string(i) + "]";
        shader->SetFloat3("u_PointLightPositions"  + idx, l.Position);
        shader->SetFloat3("u_PointLightColors"     + idx, l.Color);
        shader->SetFloat ("u_PointLightIntensities"+ idx, l.Intensity);
        shader->SetFloat ("u_PointLightRadii"      + idx, l.Radius);
    }

    mesh.Render(shader);
    s_DrawCalls += mesh.GetSubmeshCount();
    s_Triangles += mesh.GetIndexCount() / 3;

    if (s_DrawNormals && s_Data->NormalsSkinnedShader)
    {
        s_Data->NormalsSkinnedShader->Bind();
        s_Data->NormalsSkinnedShader->SetMat4("u_Model", transform);
        s_Data->NormalsSkinnedShader->SetFloat("u_NormalLength", s_NormalLength);
        mesh.Render(s_Data->NormalsSkinnedShader);
    }
}

void Renderer3D::DrawStaticMesh(StaticMesh& mesh, const glm::mat4& transform)
{
    Ref<Shader> shader;
    if (s_Data->OverrideShader)
    {
        shader = s_Data->OverrideShader;
    }
    else
    {
        HMN_CORE_ASSERT(s_Data->MeshVariants, "Renderer3D: variants not loaded — was InitForwardPlus() called?");
        uint32_t perm = mesh.GetPermutationFlags();  // includes ShaderPerm_ForwardPlus
        if (s_Scene->EnvMapID != 0u) perm |= ShaderPerm_HasEnvMap;
        perm   = ResolvePermutation(perm, k_MeshPerms, std::size(k_MeshPerms));
        shader = s_Data->MeshVariants->GetVariant(perm);
    }
    HMN_CORE_ASSERT(shader, "Renderer3D::DrawStaticMesh: no shader variant available");

    shader->Bind();

    if (s_Scene->EnvMapID != 0u)
        RenderCommand::BindTexture(3, s_Scene->EnvMapID);

    const Material& mat = mesh.GetMaterial();
    shader->SetFloat("u_Roughness", mat.Roughness);
    shader->SetFloat("u_Metalness", mat.Metalness);

    mesh.Draw(shader, transform);
    s_DrawCalls += (uint32_t)mesh.GetDrawGroupCount();
    s_Triangles += mesh.GetTriangleCount();

    if (s_DrawNormals && s_Data->NormalsShader)
    {
        s_Data->NormalsShader->Bind();
        s_Data->NormalsShader->SetFloat("u_NormalLength", s_NormalLength);
        mesh.Draw(s_Data->NormalsShader, transform);
    }

    if (s_DrawAABB && s_Data->DebugAABBShader)
    {
        const glm::vec3 mn = mesh.GetAABBMin();
        const glm::vec3 mx = mesh.GetAABBMax();
        const glm::vec3 corners[8] = {
            glm::vec3(transform * glm::vec4(mn.x, mn.y, mn.z, 1.f)),
            glm::vec3(transform * glm::vec4(mx.x, mn.y, mn.z, 1.f)),
            glm::vec3(transform * glm::vec4(mx.x, mx.y, mn.z, 1.f)),
            glm::vec3(transform * glm::vec4(mn.x, mx.y, mn.z, 1.f)),
            glm::vec3(transform * glm::vec4(mn.x, mn.y, mx.z, 1.f)),
            glm::vec3(transform * glm::vec4(mx.x, mn.y, mx.z, 1.f)),
            glm::vec3(transform * glm::vec4(mx.x, mx.y, mx.z, 1.f)),
            glm::vec3(transform * glm::vec4(mn.x, mx.y, mx.z, 1.f)),
        };
        s_Data->DebugVBO->SetData(corners, sizeof(corners));
        s_Data->DebugAABBShader->Bind();
        s_Data->DebugAABBShader->SetFloat4("u_Color", glm::vec4(1.f, 0.f, 0.f, 1.f));
        s_Data->DebugVAO->Bind();
        RenderCommand::SetDepthTestEnabled(false);
        RenderCommand::DrawIndexedLines(s_Data->DebugVAO, 24);
        RenderCommand::SetDepthTestEnabled(true);
        s_Data->DebugVAO->Unbind();
    }
}

void Renderer3D::DrawDebugLights(const std::vector<Light>& lights)
{
    if (lights.empty() || !s_Data->DebugSphereShader) return;

    s_Data->DebugSphereShader->Bind();
    s_Data->DebugSphereShader->SetFloat("u_TessLevel", 16.f);
    s_Data->DebugVAO->Bind();

    RenderCommand::SetDepthTestEnabled(false);
    for (const auto& light : lights)
    {
        glm::vec3 pos = light.Position;
        s_Data->DebugVBO->SetData(&pos, sizeof(glm::vec3));
        s_Data->DebugSphereShader->SetFloat4("u_Color", glm::vec4(light.Color, 1.f));
        float gizmoRadius = glm::clamp(light.Radius * 0.08f, 0.05f, 0.4f);
        s_Data->DebugSphereShader->SetFloat("u_Radius", gizmoRadius);
        RenderCommand::DrawPatches(1, 1);
    }
    RenderCommand::SetDepthTestEnabled(true);
    s_Data->DebugVAO->Unbind();
}

void Renderer3D::Draw(const MeshRendererComponent& rc, const glm::mat4& transform)
{
    HMN_CORE_ASSERT(rc.Mesh, "Renderer3D::Draw called with null Mesh");
    if (rc.Shader) rc.Mesh->SetShader(rc.Shader);
    DrawSkinnedMesh(*rc.Mesh, transform);
}

}
