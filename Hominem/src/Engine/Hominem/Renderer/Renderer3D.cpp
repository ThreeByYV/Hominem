#include "hmnpch.h"
#include "Renderer3D.h"
#include "Hominem/Utils/Renderer.h"
#include "RenderCommand.h"

namespace Hominem {

    Renderer3DStorage*     Renderer3D::s_Data        = nullptr;
    Renderer3D::SceneData* Renderer3D::s_Scene       = nullptr;
    bool                   Renderer3D::s_DrawNormals  = false;
    float                  Renderer3D::s_NormalLength = 0.1f;

    void Renderer3D::Init()
    {
        s_Data = new Renderer3DStorage();
        s_Scene = new SceneData();

        s_Data->ShaderLibrary = std::make_shared<ShaderLibrary>();

        s_Data->ShaderLibrary->Load("Resources/Shaders/basic.glsl");
        s_Data->ShaderLibrary->Load("Resources/Shaders/fog.glsl");
        s_Data->ShaderLibrary->Load("Resources/Shaders/skinning.glsl");
        s_Data->ShaderLibrary->Load("Resources/Shaders/static_mesh.glsl");
        s_Data->ShaderLibrary->Load("Resources/Shaders/normals_debug.glsl");
        s_Data->ShaderLibrary->Load("Resources/Shaders/normals_debug_skinned.glsl");

        s_Data->DefaultShader        = s_Data->ShaderLibrary->Get("basic");
        s_Data->NormalsShader        = s_Data->ShaderLibrary->Get("normals_debug");
        s_Data->NormalsSkinnedShader = s_Data->ShaderLibrary->Get("normals_debug_skinned");
    }

    void Renderer3D::Shutdown()
    {
        delete s_Scene; s_Scene = nullptr;
        delete s_Data;  s_Data = nullptr;
    }

    void Renderer3D::BeginScene(const glm::mat4& viewProj, const glm::vec3& cameraWorldPos)
    {
        HMN_CORE_ASSERT(s_Scene, "Renderer3D::BeginScene called before Init()");

        s_Scene->ViewProjection = viewProj;

        s_Scene->CameraWorldPos = cameraWorldPos;
    }

    void Renderer3D::EndScene()
    {
        Shader::UnbindAll();
        Texture::UnbindAll();
    }

    void Renderer3D::DrawSkinnedMesh(SkinnedMesh& mesh, const glm::mat4& transform)
    {
        Ref<Shader> shader = SelectShader(mesh.GetShader(), s_Data->OverrideShader, s_Data->DefaultShader);
        HMN_CORE_ASSERT(shader, "Renderer3D has no shader to use");

        shader->Bind();

        // All 3D shaders require these uniforms
        shader->SetMat4("u_ViewProjection", s_Scene->ViewProjection);
        shader->SetMat4("u_Model", transform);
        shader->SetFloat3("gCameraWorldPos", s_Scene->CameraWorldPos);

        mesh.Render(shader);

        if (s_DrawNormals && s_Data->NormalsSkinnedShader)
        {
            // SSBOs 4 & 5 (skinned pos/norm) are still bound from DispatchSkinning
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
        auto shader = s_Data->OverrideShader
            ? s_Data->OverrideShader
            : s_Data->ShaderLibrary->Get("static_mesh");
        HMN_CORE_ASSERT(shader, "Renderer3D: static_mesh shader not loaded");

        shader->Bind();
        shader->SetMat4("u_ViewProjection", s_Scene->ViewProjection);
        mesh.Draw(shader, transform);

        if (s_DrawNormals && s_Data->NormalsShader)
        {
            s_Data->NormalsShader->Bind();
            s_Data->NormalsShader->SetMat4("u_ViewProjection", s_Scene->ViewProjection);
            s_Data->NormalsShader->SetFloat("u_NormalLength", s_NormalLength);
            s_Data->NormalsShader->SetFloat3("u_CameraWorldPos", s_Scene->CameraWorldPos);
            mesh.Draw(s_Data->NormalsShader, transform);
        }
    }

    void Renderer3D::Draw(const MeshRendererComponent& rc, const glm::mat4& transform)
    {
        HMN_CORE_ASSERT(rc.Mesh, "Renderer3D::Draw called with null Mesh");

        // Prefer the component's per-object shader if provided
        if (rc.Shader) rc.Mesh->SetShader(rc.Shader);
        DrawSkinnedMesh(*rc.Mesh, transform);
    }

}
