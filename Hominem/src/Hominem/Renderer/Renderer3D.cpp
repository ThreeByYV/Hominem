#include "hmnpch.h"
#include "Renderer3D.h"
#include "Hominem/Utils/Renderer.h"
#include "RenderCommand.h"

namespace Hominem {

    Renderer3DStorage* Renderer3D::s_Data = nullptr;
    Renderer3D::SceneData* Renderer3D::s_Scene = nullptr;

    void Renderer3D::Init()
    {
        s_Data = new Renderer3DStorage();
        s_Scene = new SceneData();

        s_Data->ShaderLibrary = std::make_shared<ShaderLibrary>();

        s_Data->ShaderLibrary->Load("src/Hominem/Resources/Shaders/fog.glsl");
        s_Data->ShaderLibrary->Load("src/Hominem/Resources/Shaders/skinning.glsl");

        s_Data->DefaultShader = s_Data->ShaderLibrary->Get("skinning");
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

    void Renderer3D::DrawBasicMesh(BasicMesh& mesh, const glm::mat4& transform)
    {
        Ref<Shader> shader = SelectShader(mesh.GetShader(), s_Data->OverrideShader, s_Data->DefaultShader);
        HMN_CORE_ASSERT(shader, "Renderer3D has no shader to use");

        shader->Bind();

        // all 3D shaders will have these layout attributes
        shader->SetMat4("u_ViewProjection", s_Scene->ViewProjection);
        shader->SetMat4("u_Model", transform);
        shader->SetFloat3("gCameraWorldPos", s_Scene->CameraWorldPos);

        // Set other shared uniforms/textures here as needed

        mesh.Render(shader);
    }

    void Renderer3D::Draw(const MeshRendererComponent& rc, const glm::mat4& transform)
    {
        HMN_CORE_ASSERT(rc.Mesh, "Renderer3D::Draw called with null Mesh");
       
        // Prefer the component’s per-object shader if provided
        if (rc.Shader) rc.Mesh->SetShader(rc.Shader);
        DrawBasicMesh(*rc.Mesh, transform);
    }

}
