#pragma once

#include <glm/glm.hpp>
#include "VertexArray.h"

namespace Hominem {

	class RendererAPI
	{
	public:

		enum class API
		{
			None = 0, OpenGL
		};

	public:
		virtual void Init() = 0;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

		virtual void SetClearColor(const glm::vec4& color) = 0;
		virtual void Clear() = 0;

		virtual void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray) = 0;
		virtual void DrawIndexedLines(const std::shared_ptr<VertexArray>& vertexArray, uint32_t indexCount) = 0;

		virtual void SetDepthTestEnabled(bool enabled) = 0;

		// PVP / indirect batch drawing
		virtual void BindEmptyVAO()                       = 0;
		virtual void UnbindVAO()                          = 0;
		virtual void DrawArraysIndirect(uint32_t offset)  = 0;

		inline static API GetAPI() { return s_API; }
	private:
		static API s_API; //this will represent what API is active at the the current moment
	};
}