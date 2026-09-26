#pragma once

#include "Hominem/Renderer/RHI/Framebuffer.h"

namespace Hominem {

	class OpenGLFramebuffer : public Framebuffer
	{
	public:
		OpenGLFramebuffer(const FramebufferSpecification& spec);
		virtual ~OpenGLFramebuffer();

		void Invalidate();

		void Bind() override;
		void Unbind() override;
		void Resize(uint32_t width, uint32_t height) override;
		void BlitToDefault(uint32_t screenW, uint32_t screenH) override;

		uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const override { return m_ColorAttachments[index]; }
		uint32_t GetDepthAttachmentRendererID() const override { return m_DepthAttachment; }
		uint32_t GetRendererID() const override { return m_RendererID; }
		const FramebufferSpecification& GetSpecification() const override { return m_Spec; }
	private:
		uint32_t m_RendererID;
		uint32_t m_ColorAttachments[4] = {};  // up to 4 MRT attachments
		uint32_t m_DepthAttachment     = 0;
		FramebufferSpecification m_Spec;
	};
}


