#include "hmnpch.h"
#include "Renderer3D.h"
#include "Hominem/Utils/Renderer.h"
#include "RenderCommand.h"

namespace Hominem {

    Renderer3DStorage*     Renderer3D::s_Data        = nullptr;
    Renderer3D::SceneData* Renderer3D::s_Scene       = nullptr;
    bool                   Renderer3D::s_DrawNormals  = false;
    float                  Renderer3D::s_NormalLength = 0.1f;
    bool                   Renderer3D::s_DrawAABB     = false;

    void Renderer3D::Init()
    {
        s_Data  = new Renderer3DStorage();
        s_Scene = new SceneData();

        s_Data->ShaderLibrary = CreateRef<ShaderLibrary>();

        s_Data->ShaderLibrary->Load("Resources/Shaders/basic.glsl");
        s_Data->ShaderLibrary->Load("Resources/Shaders/fog.glsl");
        s_Data->ShaderLibrary->Load("Resources/Shaders/skinning.glsl");
        s_Data->ShaderLibrary->Load("Resources/Shaders/static_mesh.glsl");
        s_Data->ShaderLibrary->Load("Resources/Shaders/normals_debug.glsl");
        s_Data->ShaderLibrary->Load("Resources/Shaders/normals_debug_skinned.glsl");
        s_Data->ShaderLibrary->Load("Resources/Shaders/debug_aabb.glsl");
        s_Data->ShaderLibrary->Load("Resources/Shaders/composite.glsl");
        s_Data->ShaderLibrary->Load("Resources/Shaders/bloom_threshold.glsl");
        s_Data->ShaderLibrary->Load("Resources/Shaders/bloom_blur.glsl");

        s_Data->DefaultShader        = s_Data->ShaderLibrary->Get("basic");
        s_Data->StaticMeshShader     = s_Data->ShaderLibrary->Get("static_mesh");
        s_Data->SkinnedMeshShader    = s_Data->ShaderLibrary->Get("skinning");
        s_Data->NormalsShader        = s_Data->ShaderLibrary->Get("normals_debug");
        s_Data->NormalsSkinnedShader = s_Data->ShaderLibrary->Get("normals_debug_skinned");
        s_Data->DebugAABBShader      = s_Data->ShaderLibrary->Get("debug_aabb");

        // Debug AABB: 8 corner vertices (dynamic) + 24 indices for 12 edges (static)
        //
        //     7----6
        //    /|   /|
        //   4----5 |
        //   | 3--|-2
        //   |/   |/
        //   0----1
        static const uint32_t k_EdgeIndices[24] = {
            0,1, 1,2, 2,3, 3,0,  // bottom face
            4,5, 5,6, 6,7, 7,4,  // top face
            0,4, 1,5, 2,6, 3,7   // vertical edges
        };

        s_Data->DebugVBO = VertexBuffer::Create(8 * sizeof(glm::vec3));
        s_Data->DebugVBO->SetLayout({{ ShaderDataType::Float3, "a_Position" }});
        s_Data->DebugIBO = IndexBuffer::Create(const_cast<uint32_t*>(k_EdgeIndices), 24);
        s_Data->DebugVAO = VertexArray::Create();
        s_Data->DebugVAO->AddVertexBuffer(s_Data->DebugVBO);
        s_Data->DebugVAO->SetIndexBuffer(s_Data->DebugIBO);
    }

    void Renderer3D::Shutdown()
    {
        delete s_Scene; s_Scene = nullptr;
        delete s_Data;  s_Data  = nullptr;
    }

    void Renderer3D::BeginScene(const glm::mat4& viewProj, const glm::vec3& cameraWorldPos,
                                const DirectionalLight& light)
    {
        HMN_CORE_ASSERT(s_Scene, "Renderer3D::BeginScene called before Init()");
        s_Scene->ViewProjection = viewProj;
        s_Scene->CameraWorldPos = cameraWorldPos;
        s_Scene->Light          = light;
        s_Scene->BoundShader    = nullptr;
    }

    void Renderer3D::EndScene()
    {
        Shader::UnbindAll();
        Texture::UnbindAll();
    }

    void Renderer3D::DrawSkinnedMesh(SkinnedMesh& mesh, const glm::mat4& transform)
    {
        Ref<Shader> shader = SelectShader(mesh.GetShader(), s_Data->OverrideShader, s_Data->SkinnedMeshShader);
        HMN_CORE_ASSERT(shader, "Renderer3D has no shader to use");

        const DirectionalLight& l = s_Scene->Light;
        shader->Bind();
        shader->SetMat4("u_ViewProjection",    s_Scene->ViewProjection);
        shader->SetMat4("u_Model",             transform);
        shader->SetFloat3("u_CameraWorldPos",  s_Scene->CameraWorldPos);
        shader->SetFloat3("u_LightDirection",  l.Direction);
        shader->SetFloat3("u_LightColor",      l.Color);
        shader->SetFloat("u_AmbientIntensity", l.AmbientIntensity);
        shader->SetFloat("u_DiffuseIntensity", l.DiffuseIntensity);
        const Material& mat = mesh.GetMaterial();
        shader->SetFloat("u_Roughness", mat.Roughness);
        shader->SetFloat("u_Metalness", mat.Metalness);

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
        HMN_CORE_ASSERT(shader, "Renderer3D: static_mesh shader not loaded");

        // Only bind + set VP uniform if shader changed — state cache avoids redundant GL calls.
        if (shader.get() != s_Scene->BoundShader)
        {
            const DirectionalLight& l = s_Scene->Light;
            shader->Bind();
            shader->SetMat4("u_ViewProjection",    s_Scene->ViewProjection);
            shader->SetFloat3("u_CameraWorldPos",  s_Scene->CameraWorldPos);
            shader->SetFloat3("u_LightDirection",  l.Direction);
            shader->SetFloat3("u_LightColor",      l.Color);
            shader->SetFloat("u_AmbientIntensity", l.AmbientIntensity);
            shader->SetFloat("u_DiffuseIntensity", l.DiffuseIntensity);
            s_Scene->BoundShader = shader.get();
        }

        // Material — per-mesh, set every draw call.
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

            s_Data->DebugVBO->SetData(corners, static_cast<uint32_t>(sizeof(corners)));

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

    void Renderer3D::Draw(const MeshRendererComponent& rc, const glm::mat4& transform)
    {
        HMN_CORE_ASSERT(rc.Mesh, "Renderer3D::Draw called with null Mesh");
        if (rc.Shader) rc.Mesh->SetShader(rc.Shader);
        DrawSkinnedMesh(*rc.Mesh, transform);
    }

}
