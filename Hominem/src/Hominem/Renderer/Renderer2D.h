#pragma once

#include "Camera.h"

namespace Hominem {

	class Renderer2D
	{
	public: 
		// Core Rendering
		static void Init();
		static void Shutdown();

		static void BeginScene(OrthographicCamera& camera);
		static void EndScene();

		// Primitives
		static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
		
		//z-axis can be used for depth features
		static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);

	};
}