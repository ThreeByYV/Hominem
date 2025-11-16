#pragma once

#include "Shader.h"
#include "Hominem/Scene/BasicMesh.h" 

namespace Hominem {

	struct MeshRendererComponent
	{
		Ref<BasicMesh> Mesh;   // required
		Ref<Shader>    Shader; // optional per-object
	};

	struct Renderer3DStorage
	{
		Ref<ShaderLibrary> ShaderLibrary;
		Ref<Shader>        DefaultShader;
		Ref<Shader>        OverrideShader; // optional scene-wide
	};

	class Renderer3D
	{
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const glm::mat4& viewProj, const glm::vec3& cameraWorldPos);
		static void EndScene();

		// Optional scene-wide override (shadow pass, sunset, etc.)
		static void SetOverrideShader(const Ref<Shader>& shader) { s_Data->OverrideShader = shader; }
		static void ClearOverrideShader() { s_Data->OverrideShader.reset(); }

		static void DrawBasicMesh(BasicMesh& mesh, const glm::mat4& transform); // todo stop using a pure OpenGL only class here

		static void Draw(const MeshRendererComponent& rc, const glm::mat4& transform);

		static Ref<ShaderLibrary> GetShaderLibrary() { return s_Data->ShaderLibrary; };

	private:
		struct SceneData 
		{
			glm::mat4 ViewProjection{};
			glm::vec3 CameraWorldPos{};
		};

		static Renderer3DStorage* s_Data;
		static SceneData* s_Scene;
	};
}