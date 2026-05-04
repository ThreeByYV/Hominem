#pragma once

#include "Texture.h"
#include "Camera.h"
#include "Shader.h"
#include "Font.h"
#include "Quad.h"
#include "VertexArray.h"
#include "SubTexture2D.h"
#include "StorageBuffer.h"

namespace Hominem {

	struct TextVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
	};

	// One entry per quad in the SSBO — GPU reads this in the batch vertex shader
	struct QuadSSBOData
	{
		glm::mat4 Transform;  // 64 bytes
		glm::vec4 Color;      // 16 bytes
		glm::vec2 UVMin;      //  8 bytes
		glm::vec2 UVMax;      //  8 bytes — total 96 bytes, std430-compatible
	};

	struct Renderer2DStorage
	{
		glm::mat4 ViewProjectionMatrix;

		static const uint32_t MaxQuads = 10000;
		Ref<StorageBuffer>     QuadSSBO;
		Ref<DrawIndirectBuffer> IndirectBuffer;
		std::vector<QuadSSBOData> QuadBatch; // all quads accumulated this frame

		// One group per contiguous run of quads sharing the same texture
		struct TextureGroup
		{
			Ref<Texture2D> Texture;
			uint32_t       FirstQuad = 0; // index into QuadBatch
			uint32_t       QuadCount = 0;
		};
		std::vector<TextureGroup> TextureGroups;

		Ref<Shader> BatchPVPShader;

		Ref<VertexArray>  QuadVertexArray;
		Ref<VertexBuffer> QuadVBO;
		Ref<IndexBuffer>  QuadIBO;

		Ref<Shader> DefaultShader;
		Ref<Shader> OverrideShader;
		Ref<Shader> TextureShader;

		// Text
		Ref<VertexArray>  TextVertexArray;
		Ref<VertexBuffer> TextVertexBuffer;
		Ref<Shader>       TextShader;
		Ref<Texture2D>    FontAtlasTexture;
		uint32_t TextVertexCount = 0;
		TextVertex* TextVertexBufferBase = nullptr;
		TextVertex* TextVertexBufferPtr  = nullptr;

		Ref<ShaderLibrary> ShaderLibrary;
		Ref<Texture2D>     WhiteTexture;
	};

	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(OrthographicCamera& camera); //todo remove
		static void BeginScene(Camera& camera, const glm::mat4& transform);
		
		template<typename CameraType>
		static void BeginScene(const CameraType& camera)
		{
			s_Data->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
			s_Data->QuadBatch.clear();
			s_Data->TextureGroups.clear();
			s_Data->TextureShader->Bind();
			s_Data->TextureShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
		}

		static Ref<ShaderLibrary> GetShaderLibrary();

		static void EndScene();
		static void Flush(); // force-flush pending batch (e.g. before a render-state change)

		// Primitives
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color); // z-axis in the position can be used for depth features
		
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture);
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, const glm::vec4& tint);
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
		static void PushQuad(const glm::mat4& transform, const glm::vec4& color,
		                     const glm::vec2& uvMin, const glm::vec2& uvMax,
		                     const Ref<Texture2D>& texture);

	};
}
