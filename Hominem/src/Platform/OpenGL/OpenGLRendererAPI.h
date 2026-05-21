#pragma once

#include "Hominem/Renderer/RendererAPI.h"

namespace Hominem {

	class OpenGLRendererAPI : public RendererAPI
	{
	public:
		void Init() override;

		void SetClearColor(const glm::vec4& color) override;
		void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

		void Clear() override;

		void DrawIndexed(const Ref<VertexArray>& vertexArray) override;
		void DrawIndexedLines(const Ref<VertexArray>& vertexArray, uint32_t indexCount) override;

		void SetDepthTestEnabled(bool enabled) override;
		void SetScissorEnabled(bool enabled)   override;

		void BindEmptyVAO()                      override;
		void UnbindVAO()                         override;
		void DrawArraysIndirect(uint32_t offset) override;
		void DrawFullscreenTriangle()                        override;
		void DrawPoints(uint32_t count, float pointSize)     override;
		void DrawPatches(uint32_t count, uint32_t patchVertices) override;
		void SetWireframe(bool enabled)                      override;
		void BindTexture(uint32_t slot, uint32_t id)         override;

	private:
		uint32_t m_EmptyVAO = 0;
	};
}