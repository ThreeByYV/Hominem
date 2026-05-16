#include "hmnpch.h"
#include "OpenGLFramebuffer.h"
#include <glad/glad.h>

namespace Hominem {

	OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpecification& spec)
		: m_Spec(spec)
	{
		Invalidate();
	}

	void OpenGLFramebuffer::Invalidate()
	{
		bool hdr = (m_Spec.Format == FramebufferFormat::RGBA16F);

		glCreateFramebuffers(1, &m_RendererID);

		glCreateTextures(GL_TEXTURE_2D, 1, &m_ColorAttachment);
		glTextureStorage2D(m_ColorAttachment, 1, hdr ? GL_RGBA16F : GL_RGBA8, m_Spec.Width, m_Spec.Height);
		glTextureParameteri(m_ColorAttachment, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_ColorAttachment, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glNamedFramebufferTexture(m_RendererID, GL_COLOR_ATTACHMENT0, m_ColorAttachment, 0);

		glCreateTextures(GL_TEXTURE_2D, 1, &m_DepthAttachment);
		glTextureStorage2D(m_DepthAttachment, 1, GL_DEPTH24_STENCIL8, m_Spec.Width, m_Spec.Height);
		glNamedFramebufferTexture(m_RendererID, GL_DEPTH_STENCIL_ATTACHMENT, m_DepthAttachment, 0);

		HMN_CORE_ASSERT(glCheckNamedFramebufferStatus(m_RendererID, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
			"Framebuffer is incomplete!");
	}

	void OpenGLFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
			return;

		m_Spec.Width  = width;
		m_Spec.Height = height;

		glDeleteFramebuffers(1, &m_RendererID);
		glDeleteTextures(1, &m_ColorAttachment);
		glDeleteTextures(1, &m_DepthAttachment);

		Invalidate();
	}

	void OpenGLFramebuffer::Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
	}

	void OpenGLFramebuffer::Unbind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLFramebuffer::BlitToDefault(uint32_t screenW, uint32_t screenH)
	{
		glBlitNamedFramebuffer(m_RendererID, 0,
			0, 0, m_Spec.Width, m_Spec.Height,
			0, 0, screenW,      screenH,
			GL_COLOR_BUFFER_BIT, GL_LINEAR);
	}

	OpenGLFramebuffer::~OpenGLFramebuffer()
	{
		glDeleteFramebuffers(1, &m_RendererID);
	}


}