#pragma once

#include "OrthographicCamera.h"
#include "Texture.h"
#include "Hominem/Renderer/Camera.h"
#include "Shader.h"

#include "VertexArray.h"
#include "Shader.h"
#include "Texture.h"

namespace Hominem {

	struct Renderer2DStorage
	{
		Ref<VertexArray> QuadVertexArray;
		Ref<Shader> TextureShader;
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

		static void BeginScene(OrthographicCamera& camera); //todo remove
		static void BeginScene(Camera& camera, const glm::mat4& transform);
		
		template<typename CameraType>
		static void BeginScene(const CameraType& camera)
		{
			s_Data->TextureShader->Bind();
			s_Data->TextureShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
		}

		static void EndScene();

		static Ref<ShaderLibrary> GetShaderLibrary();

		// Primitives
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color); //z-axis in the position can be used for depth features
		
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture);

		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture);
		
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, const glm::vec4& tint);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, const glm::vec4& tint);

	private:
		static Renderer2DStorage* s_Data;
	};


}