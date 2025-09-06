#pragma once

#include "OrthographicCamera.h"
#include "Texture.h"
#include "Hominem/Renderer/Camera.h"

namespace Hominem {

	class Renderer2D
	{
	public: 
		static void Init();
		static void Shutdown();

		static void BeginScene(OrthographicCamera& camera); //todo remove
		static void BeginScene(Camera& camera, const glm::mat4& transform);

		static void EndScene();

		// Primitives
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color); //z-axis in the position can be used for depth features
		
		static void DrawQuad(const glm::mat4& transform, const glm::vec4& color);
		static void DrawQuad(const glm::mat4& transform, const Ref<Texture2D>& texture);

		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture);
		
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, const glm::vec4& tint);
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, const glm::vec4& tint);

	};
}