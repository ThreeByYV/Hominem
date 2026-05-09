#pragma once

#include "Shader.h"
#include "Buffer.h"
#include "VertexArray.h"
#include "Hominem/Renderer/SkinnedMesh.h"
#include "Hominem/Renderer/StaticMesh.h"
#include "Hominem/Renderer/Frustum.h"

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
		Ref<Shader>        OverrideShader;        // optional scene-wide
		Ref<Shader>        StaticMeshShader;
		Ref<Shader>        NormalsShader;
		Ref<Shader>        NormalsSkinnedShader;
		Ref<Shader>        DebugAABBShader;
		Ref<VertexArray>   DebugVAO;
		Ref<VertexBuffer>  DebugVBO;              // dynamic — 8 world-space corners per draw
		Ref<IndexBuffer>   DebugIBO;              // static  — 24 indices for 12 edges
	};

	class Renderer3D
	{
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const glm::mat4& viewProj, const glm::vec3& cameraWorldPos);
		static void EndScene();

		static void SetOverrideShader(const Ref<Shader>& shader) { s_Data->OverrideShader = shader; }
		static void ClearOverrideShader() { s_Data->OverrideShader.reset(); }

		// Debug normal visualization — toggle with N key
		static void SetDrawNormals(bool enabled) { s_DrawNormals = enabled; }
		static bool GetDrawNormals()             { return s_DrawNormals; }
		static void SetNormalLength(float len)   { s_NormalLength = len; }

		// Debug AABB wireframe — toggle with B key
		static void SetDrawAABB(bool enabled) { s_DrawAABB = enabled; }
		static bool GetDrawAABB()             { return s_DrawAABB; }

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
		static bool               s_DrawAABB;
	};
}
