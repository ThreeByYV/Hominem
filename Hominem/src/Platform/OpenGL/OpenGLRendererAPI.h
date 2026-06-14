#pragma once

#include "Hominem/Renderer/RendererAPI.h"

namespace Hominem {

	class OpenGLRendererAPI : public RendererAPI
	{
	public:
		void Init()                          override;
		void ApplyState(const PipelineState& s) override;

		void SetClearColor(const glm::vec4& color) override;
		void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

		void Clear() override;

		void DrawIndexed(const Ref<VertexArray>& vertexArray) override;
		void DrawIndexedLines(const Ref<VertexArray>& vertexArray, uint32_t indexCount) override;

		void SetDepthTestEnabled(bool enabled)  override;
		void SetDepthWriteEnabled(bool enabled) override;
		void SetBlendMode(BlendMode mode)       override;
		void SetCullFaceEnabled(bool enabled)   override;
		void SetScissorEnabled(bool enabled)   override;

		void BindEmptyVAO()                      override;
		void UnbindVAO()                         override;
		void DrawArraysIndirect(uint32_t offset) override;
		void DrawFullscreenTriangle()                        override;
		void DrawUnitQuad()                                  override;
		void DrawPoints(uint32_t count, float pointSize)     override;
		void DrawPatches(uint32_t count, uint32_t patchVertices) override;
		void SetWireframe(bool enabled)                      override;
		void BindTexture(uint32_t slot, uint32_t id)         override;

		void BindVertexArray(uint32_t vaoID) override;
		void DrawElementsBaseVertex(uint32_t indexCount, uint32_t indexByteOffset, int32_t baseVertex) override;
		void UpdateBufferSubData(uint32_t bufferID, const void* data, uint32_t size, uint32_t offset) override;
		void BindShaderStorageBufferBase(uint32_t bufferID, uint32_t slot) override;
		void BindDrawIndirectBuffer(uint32_t bufferID) override;
		void MultiDrawElementsIndirect(uint32_t byteOffset, uint32_t drawCount, uint32_t stride) override;

		const char* GetGPUVendor()   const override;
		const char* GetGPURenderer() const override;

		uint32_t GenFramebuffer()                                    override;
		void     BindFramebuffer(uint32_t id)                        override;
		void     UnbindFramebuffer()                                 override;
		void     DeleteFramebuffer(uint32_t id)                      override;
		void     AttachCubeFace(uint32_t cubeID, int face, int mip)  override;
		void     Attach2DTexture(uint32_t texID)                     override;

		uint32_t GenRenderbuffer()                                   override;
		void     BindRenderbuffer(uint32_t id)                       override;
		void     RenderbufferDepth(uint32_t width, uint32_t height)  override;
		void     AttachRenderbuffer(uint32_t rboID)                  override;
		void     DeleteRenderbuffer(uint32_t id)                     override;

	private:
		uint32_t m_EmptyVAO = 0;
	};
}