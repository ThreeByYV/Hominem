#include "hmnpch.h"
#include "Renderer3D.h"
#include "Hominem/Utils/Renderer.h"
#include "RenderCommand.h"

#include <ranges>
#include <glm/gtc/matrix_inverse.hpp>

namespace Hominem {

namespace {

    struct alignas(16) GPUPointLight
    {
        glm::vec4 positionAndRadius; // xyz = world pos, w = radius
        glm::vec4 colorAndIntensity; // xyz = colour,    w = intensity
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

void Renderer3D::Init()
{
    s_Data  = new Renderer3DStorage();
    s_Scene = new SceneData();

    s_Data->ShaderLibrary = CreateRef<ShaderLibrary>();

    s_Data->ShaderLibrary->Load("Resources/Shaders/basic.glsl");
    s_Data->ShaderLibrary->Load("Resources/Shaders/fog.glsl");
    s_Data->ShaderLibrary->Load("Resources/Shaders/skinning.glsl");
    s_Data->ShaderLibrary->Load("Resources/Shaders/normals_debug.glsl");
    s_Data->ShaderLibrary->Load("Resources/Shaders/normals_debug_skinned.glsl");
    s_Data->ShaderLibrary->Load("Resources/Shaders/debug_aabb.glsl");
    s_Data->ShaderLibrary->Load("Resources/Shaders/debug_sphere.glsl");
    s_Data->ShaderLibrary->Load("Resources/Shaders/composite.glsl");
    s_Data->ShaderLibrary->Load("Resources/Shaders/bloom_threshold.glsl");
    s_Data->ShaderLibrary->Load("Resources/Shaders/bloom_blur.glsl");

    s_Data->DefaultShader        = s_Data->ShaderLibrary->Get("basic");
    s_Data->SkinnedMeshShader    = s_Data->ShaderLibrary->Get("skinning");
    s_Data->NormalsShader        = s_Data->ShaderLibrary->Get("normals_debug");
    s_Data->NormalsSkinnedShader = s_Data->ShaderLibrary->Get("normals_debug_skinned");
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

    s_Data->LightBuffer        = StorageBuffer::Create(MAX_LIGHTS * sizeof(GPUPointLight));
    s_Data->GlobalLightCounter = StorageBuffer::Create(sizeof(uint32_t));

    // LightIndexList and LightGrid are sized per-viewport in ResizeTileBuffers().

    s_Data->LightCullingShader = ComputeShader::Create("Resources/Shaders/light_culling.comp");

    s_Data->ShaderLibrary->Load("Resources/Shaders/forward_plus.glsl");
    s_Data->StaticMeshShader = s_Data->ShaderLibrary->Get("forward_plus");

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
        static_cast<uint32_t>(std::min(frame.pointLights.size(), (size_t)MAX_LIGHTS));

    std::vector<GPUPointLight> gpuLights;
    gpuLights.reserve(lightCount);
    std::ranges::transform(
        frame.pointLights | std::views::take(lightCount),
        std::back_inserter(gpuLights),
        [](const PointLight& l) -> GPUPointLight {
            return { { l.Position, l.Radius }, { l.Color, l.Intensity } };
        });

    s_Data->LightBuffer->SetData(gpuLights.data(), lightCount * sizeof(GPUPointLight));

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

    s_Scene->ViewProjection = frame.viewProjection3D;
    s_Scene->CameraWorldPos = frame.cameraWorldPos;
    s_Scene->Light          = frame.light;
    s_Scene->ViewportWidth  = frame.viewportWidth;
    s_Scene->BoundShader    = nullptr;
    s_Scene->PointLights    = frame.pointLights;

    if (s_Data->LightCullingShader && !frame.pointLights.empty())
        CullLights(frame);
}

void Renderer3D::EndScene()
{
    Shader::UnbindAll();
    Texture::UnbindAll();
}

void Renderer3D::DrawSkinnedMesh(SkinnedMesh& mesh, const glm::mat4& transform)
{
    Ref<Shader> shader = SelectShader(mesh.GetShader(), s_Data->OverrideShader, s_Data->SkinnedMeshShader);
    HMN_CORE_ASSERT(shader, "Renderer3D has no shader for skinned mesh");

    const DirectionalLight& l = s_Scene->Light;
    shader->Bind();
    shader->SetMat4("u_ViewProjection",    s_Scene->ViewProjection);
    shader->SetMat4("u_Model",             transform);
    shader->SetFloat3("u_CameraWorldPos",  s_Scene->CameraWorldPos);
    shader->SetFloat3("u_LightDirection",  l.Direction);
    shader->SetFloat3("u_LightColor",      l.Color);
    shader->SetFloat3("u_AmbientColor",    l.AmbientColor);
    shader->SetFloat("u_AmbientIntensity", l.AmbientIntensity);
    shader->SetFloat("u_DiffuseIntensity", l.DiffuseIntensity);
    const Material& mat = mesh.GetMaterial();
    shader->SetFloat("u_Roughness", mat.Roughness);
    shader->SetFloat("u_Metalness", mat.Metalness);

    const int lightCount = (int)std::min(s_Scene->PointLights.size(), (size_t)MAX_POINT_LIGHTS_SKINNED);
    shader->SetInt("u_PointLightCount", lightCount);
    for (int i = 0; i < lightCount; i++)
    {
        const auto& pl  = s_Scene->PointLights[i];
        const std::string idx = "[" + std::to_string(i) + "]";
        shader->SetFloat3("u_PointLightPositions"  + idx, pl.Position);
        shader->SetFloat3("u_PointLightColors"     + idx, pl.Color);
        shader->SetFloat ("u_PointLightIntensities"+ idx, pl.Intensity);
        shader->SetFloat ("u_PointLightRadii"      + idx, pl.Radius);
    }

    mesh.Render(shader);

    if (s_DrawNormals && s_Data->NormalsSkinnedShader)
    {
        s_Data->NormalsSkinnedShader->Bind();
        s_Data->NormalsSkinnedShader->SetMat4("u_ViewProjection", s_Scene->ViewProjection);
        s_Data->NormalsSkinnedShader->SetMat4("u_Model", transform);
        s_Data->NormalsSkinnedShader->SetFloat("u_NormalLength", s_NormalLength);
        s_Data->NormalsSkinnedShader->SetFloat3("u_CameraWorldPos", s_Scene->CameraWorldPos);
        mesh.Render(s_Data->NormalsSkinnedShader);
    }
}

void Renderer3D::DrawStaticMesh(StaticMesh& mesh, const glm::mat4& transform)
{
    auto shader = s_Data->OverrideShader ? s_Data->OverrideShader : s_Data->StaticMeshShader;
    HMN_CORE_ASSERT(shader, "Renderer3D: Forward+ shader not loaded — was InitForwardPlus() called?");

    // Re-upload scene-wide uniforms only when the shader changes.
    if (shader.get() != s_Scene->BoundShader)
    {
        const DirectionalLight& l = s_Scene->Light;
        shader->Bind();
        shader->SetMat4("u_ViewProjection",    s_Scene->ViewProjection);
        shader->SetFloat3("u_CameraWorldPos",  s_Scene->CameraWorldPos);
        shader->SetFloat3("u_LightDirection",  l.Direction);
        shader->SetFloat3("u_LightColor",      l.Color);
        shader->SetFloat3("u_AmbientColor",    l.AmbientColor);
        shader->SetFloat("u_AmbientIntensity", l.AmbientIntensity);
        shader->SetFloat("u_DiffuseIntensity", l.DiffuseIntensity);
        // u_ScreenWidth lets the fragment shader derive its tile index from gl_FragCoord.
        shader->SetInt("u_ScreenWidth",  (int)s_Scene->ViewportWidth);
        shader->SetInt("u_DebugMode",    s_DebugHeatmap ? 1 : 0);
        s_Scene->BoundShader = shader.get();
    }

    const Material& mat = mesh.GetMaterial();
    shader->SetFloat("u_Roughness", mat.Roughness);
    shader->SetFloat("u_Metalness", mat.Metalness);

    mesh.Draw(shader, transform);

    if (s_DrawNormals && s_Data->NormalsShader)
    {
        s_Data->NormalsShader->Bind();
        s_Data->NormalsShader->SetMat4("u_ViewProjection", s_Scene->ViewProjection);
        s_Data->NormalsShader->SetFloat("u_NormalLength", s_NormalLength);
        s_Data->NormalsShader->SetFloat3("u_CameraWorldPos", s_Scene->CameraWorldPos);
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
        s_Data->DebugAABBShader->SetMat4("u_ViewProjection", s_Scene->ViewProjection);
        s_Data->DebugAABBShader->SetFloat4("u_Color", glm::vec4(1.f, 0.f, 0.f, 1.f));
        s_Data->DebugVAO->Bind();
        RenderCommand::SetDepthTestEnabled(false);
        RenderCommand::DrawIndexedLines(s_Data->DebugVAO, 24);
        RenderCommand::SetDepthTestEnabled(true);
        s_Data->DebugVAO->Unbind();
    }
}

void Renderer3D::DrawDebugPointLights(const std::vector<PointLight>& lights)
{
    if (lights.empty() || !s_Data->DebugSphereShader) return;

    s_Data->DebugSphereShader->Bind();
    s_Data->DebugSphereShader->SetMat4("u_ViewProjection", s_Scene->ViewProjection);
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
