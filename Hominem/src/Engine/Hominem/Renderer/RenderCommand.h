#pragma once

#include "RendererAPI.h"

namespace Hominem {

	class RenderCommand
	{
	public:
		inline static void Init()
		{
			s_RendererAPI->Init();
		}

		inline static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			s_RendererAPI->SetViewport(x, y, width, height);
		}

		inline static void SetClearColor(const glm::vec4& color)
		{
			s_RendererAPI->SetClearColor(color);
		}
	
		inline static void Clear()
		{
			s_RendererAPI->Clear();
		}

		inline static void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray)
		{
			s_RendererAPI->DrawIndexed(vertexArray);
		}

		inline static void DrawIndexedLines(const std::shared_ptr<VertexArray>& vertexArray, uint32_t indexCount)
		{
			s_RendererAPI->DrawIndexedLines(vertexArray, indexCount);
		}

		inline static void SetDepthTestEnabled(bool enabled)
		{
			s_RendererAPI->SetDepthTestEnabled(enabled);
		}

		inline static void SetScissorEnabled(bool enabled)
		{
			s_RendererAPI->SetScissorEnabled(enabled);
		}

		inline static void BindEmptyVAO()
		{
			s_RendererAPI->BindEmptyVAO();
		}

		inline static void UnbindVAO()
		{
			s_RendererAPI->UnbindVAO();
		}

		// Draws triangles using parameters from the currently bound DrawIndirectBuffer
		// at the given byte offset.
		inline static void DrawArraysIndirect(uint32_t offset)
		{
			s_RendererAPI->DrawArraysIndirect(offset);
		}

	private:
		static RendererAPI* s_RendererAPI;
	};

}

