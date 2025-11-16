#pragma once

#include "Camera.h"
#include "Texture.h"
#include "Shader.h"
#include "Font.h"
#include "Quad.h"
#include "VertexArray.h"
#include "Shader.h"
#include "Texture.h"
#include "SubTexture2D.h"

namespace Hominem {

	struct TextVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
	};

	struct Renderer2DStorage
	{
		glm::mat4 ViewProjectionMatrix;
		Ref<VertexArray> QuadVertexArray;
		static const uint32_t MaxVertices = 20000 * 4;
		float DefaultQuadVertices[5 * 4];   // pos (3) + uv (2)
		float TempQuadVertices[5 * 4];      // scratch buffer used for SetData()


		Ref<Shader> DefaultShader;
		Ref<Shader> OverrideShader;
		Ref<Shader> TextureShader;

		Ref<VertexArray> TextVertexArray;
		Ref<VertexBuffer> TextVertexBuffer;
		Ref<Shader> TextShader;
		Ref<Texture2D> FontAtlasTexture;
		uint32_t TextVertexCount = 0;
		TextVertex* TextVertexBufferBase = nullptr;
		TextVertex* TextVertexBufferPtr = nullptr;

		Ref<ShaderLibrary> ShaderLibrary;
		Ref<IndexBuffer> IndexBuffer;
		Ref<Texture2D> WhiteTexture;
		Ref<VertexBuffer> VertexBuffer;

	};

	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();

		template<typename CameraType>
		static void BeginScene(const CameraType& camera)
		{
			s_Data->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
			s_Data->TextureShader->Bind();
			s_Data->TextureShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
		}

		static void UploadQuadUVs(const glm::vec2 texCoords[4]);
		static Ref<ShaderLibrary> GetShaderLibrary();

		static void EndScene();

		// Primitives
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color); // z-axis in the position can be used for depth features
		static void DrawQuad(const Quad& q);


		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture);

		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, const glm::vec4& tint);
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<SubTexture2D>& subtexture, const glm::vec4& tint);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, const glm::vec4& tint);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<SubTexture2D>& subtexture, const glm::vec4& tint);


		static void DrawString(const std::string& string, Ref<Font> font, const glm::mat4& transform, const glm::vec4& color);

	private:
		static Renderer2DStorage* s_Data;

	private:
		static void DrawTexturedQuadInternal(
			const glm::vec3& position,
			const glm::vec2& size,
			const glm::vec4& color,
			const Ref<Texture2D>& texture,
			const glm::vec2 texCoords[4]);

	};
}
