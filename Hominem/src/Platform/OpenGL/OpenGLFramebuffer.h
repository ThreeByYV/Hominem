#pragma once

#include "Hominem/Renderer/Framebuffer.h"

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

		uint32_t GetColorAttachmentRendererID() const override { return m_ColorAttachment; } //this will be bind as a tetxure
		const FramebufferSpecification& GetSpecification() const override { return m_Spec; }
	private:
		uint32_t m_RendererID;
		uint32_t m_ColorAttachment, m_DepthAttachment;
		FramebufferSpecification m_Spec;
	};
}


