#pragma once

#include "RendererAPI.h"

namespace Hominem {

	class RenderCommand
	{
	public:
		/// Initialises the underlying RendererAPI (called once at startup).
		inline static void Init()
		{
			s_RendererAPI->Init();
		}

		/// Sets the GL viewport rectangle.
		inline static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			s_RendererAPI->SetViewport(x, y, width, height);
		}

		/// Sets the color written by the next Clear() call.
		inline static void SetClearColor(const glm::vec4& color)
		{
			s_RendererAPI->SetClearColor(color);
		}

		/// Clears the color and depth buffers.
		inline static void Clear()
		{
			s_RendererAPI->Clear();
		}

		/// Draws the indexed geometry in the given vertex array.
		inline static void DrawIndexed(const Ref<VertexArray>& vertexArray)
		{
			s_RendererAPI->DrawIndexed(vertexArray);
		}

		/// Draws indexCount indices from the given vertex array as GL_LINES.
		inline static void DrawIndexedLines(const Ref<VertexArray>& vertexArray, uint32_t indexCount)
		{
			s_RendererAPI->DrawIndexedLines(vertexArray, indexCount);
		}

		/// Enables or disables the depth test.
		inline static void SetDepthTestEnabled(bool enabled)
		{
			s_RendererAPI->SetDepthTestEnabled(enabled);
		}

		/// Enables or disables the scissor test.
		inline static void SetScissorEnabled(bool enabled)
		{
			s_RendererAPI->SetScissorEnabled(enabled);
		}

		/// Binds the engine-owned empty VAO (required by OpenGL before any draw call).
		inline static void BindEmptyVAO()
		{
			s_RendererAPI->BindEmptyVAO();
		}

		/// Unbinds the currently bound VAO.
		inline static void UnbindVAO()
		{
			s_RendererAPI->UnbindVAO();
		}

		/// Issues an indirect draw using parameters from the currently bound DrawIndirectBuffer at the given byte offset.
		inline static void DrawArraysIndirect(uint32_t offset)
		{
			s_RendererAPI->DrawArraysIndirect(offset);
		}

		/// Draws a fullscreen triangle using gl_VertexID — no VBO needed. Use for post-process passes.
		inline static void DrawFullscreenTriangle()
		{
			s_RendererAPI->DrawFullscreenTriangle();
		}

		/// Binds a texture to the given texture unit.
		inline static void BindTexture(uint32_t slot, uint32_t id)
		{
			s_RendererAPI->BindTexture(slot, id);
		}

	private:
		static RendererAPI* s_RendererAPI;
	};

}
