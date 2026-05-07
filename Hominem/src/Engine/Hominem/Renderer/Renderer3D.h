#pragma once

#include "Shader.h"
#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Renderer/StaticMesh.h"

namespace Hominem {

	struct MeshRendererComponent
	{
		Ref<SkinnedMesh> Mesh;   // required
		Ref<Shader>      Shader; // optional per-object
	};

	struct Renderer3DStorage
	{
		Ref<ShaderLibrary> ShaderLibrary;
		Ref<Shader>        DefaultShader;
		Ref<Shader>        OverrideShader; // optional scene-wide
		Ref<Shader>        NormalsShader;
		Ref<Shader>        NormalsSkinnedShader;
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

		// Debug normal visualization — toggle with N key
		static void SetDrawNormals(bool enabled) { s_DrawNormals = enabled; }
		static bool GetDrawNormals()             { return s_DrawNormals; }
		static void SetNormalLength(float len)   { s_NormalLength = len; }

		static void DrawSkinnedMesh(SkinnedMesh& mesh, const glm::mat4& transform);
		static void DrawStaticMesh(StaticMesh& mesh,  const glm::mat4& transform);

		static void Draw(const MeshRendererComponent& rc, const glm::mat4& transform);

		static Ref<ShaderLibrary> GetShaderLibrary() { return s_Data->ShaderLibrary; };

	private:
		struct SceneData 
		{
			glm::mat4 ViewProjection{};
			glm::vec3 CameraWorldPos{};
		};

		static Renderer3DStorage* s_Data;
		static SceneData*         s_Scene;
		static bool               s_DrawNormals;
		static float              s_NormalLength;
	};
}