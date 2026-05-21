#pragma once

#include <glm/glm.hpp>
#include "VertexArray.h"

namespace Hominem {

	class RendererAPI
	{
	public:
		enum class API { None = 0, OpenGL };

		/// Initialises the API and creates persistent GL objects (e.g. empty VAO).
		virtual void Init() = 0;

		/// Sets the GL viewport rectangle.
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

		/// Sets the color written by the next Clear() call.
		virtual void SetClearColor(const glm::vec4& color) = 0;

		/// Clears the color and depth buffers.
		virtual void Clear() = 0;

		/// Draws the indexed geometry in the given vertex array.
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray) = 0;

		/// Draws indexCount indices from the given vertex array as GL_LINES.
		virtual void DrawIndexedLines(const Ref<VertexArray>& vertexArray, uint32_t indexCount) = 0;

		/// Enables or disables the depth test.
		virtual void SetDepthTestEnabled(bool enabled) = 0;

		/// Enables or disables the scissor test.
		virtual void SetScissorEnabled(bool enabled) = 0;

		/// Binds the engine-owned empty VAO (required by OpenGL before any draw call).
		virtual void BindEmptyVAO() = 0;

		/// Unbinds the currently bound VAO.
		virtual void UnbindVAO() = 0;

		/// Issues an indirect draw using parameters from the currently bound DrawIndirectBuffer at the given byte offset.
		virtual void DrawArraysIndirect(uint32_t offset) = 0;

		/// Draws a fullscreen triangle using gl_VertexID — no VBO needed. Use for post-process passes.
		virtual void DrawFullscreenTriangle() = 0;

		/// Draws count vertices as points with the given pixel size.
		virtual void DrawPoints(uint32_t count, float pointSize) = 0;

		/// Draws count patches for tessellation.
		virtual void DrawPatches(uint32_t count, uint32_t patchVertices = 1) = 0;

		/// Toggles wireframe polygon mode.
		virtual void SetWireframe(bool enabled) = 0;

		/// Binds a texture to the given texture unit.
		virtual void BindTexture(uint32_t slot, uint32_t id) = 0;

		inline static API GetAPI() { return s_API; }

	private:
		static API s_API;
	};

}
