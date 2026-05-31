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
		GLenum internalFmt = (m_Spec.Format == FramebufferFormat::RGBA16F) ? GL_RGBA16F : GL_RGBA8;
		uint32_t n = std::clamp(m_Spec.NumColorAttachments, 1u, 4u);

		glCreateFramebuffers(1, &m_RendererID);

		// Color attachments (all same format)
		GLenum drawBuffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
		                          GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
		glCreateTextures(GL_TEXTURE_2D, n, m_ColorAttachments);
		for (uint32_t i = 0; i < n; ++i)
		{
			glTextureStorage2D(m_ColorAttachments[i], 1, internalFmt, m_Spec.Width, m_Spec.Height);
			glTextureParameteri(m_ColorAttachments[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(m_ColorAttachments[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(m_ColorAttachments[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTextureParameteri(m_ColorAttachments[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glNamedFramebufferTexture(m_RendererID, GL_COLOR_ATTACHMENT0 + i, m_ColorAttachments[i], 0);
		}
		glNamedFramebufferDrawBuffers(m_RendererID, n, drawBuffers);

		// Depth (always a texture so it can be sampled if needed)
		glCreateTextures(GL_TEXTURE_2D, 1, &m_DepthAttachment);
		glTextureStorage2D(m_DepthAttachment, 1, GL_DEPTH24_STENCIL8, m_Spec.Width, m_Spec.Height);
		glNamedFramebufferTexture(m_RendererID, GL_DEPTH_STENCIL_ATTACHMENT, m_DepthAttachment, 0);

		HMN_CORE_ASSERT(glCheckNamedFramebufferStatus(m_RendererID, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
			"Framebuffer is incomplete!");
	}

	void OpenGLFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) return;

		m_Spec.Width  = width;
		m_Spec.Height = height;

		glDeleteFramebuffers(1, &m_RendererID);
		m_RendererID = 0;

		uint32_t n = std::clamp(m_Spec.NumColorAttachments, 1u, 4u);
		glDeleteTextures(n, m_ColorAttachments);
		for (uint32_t i = 0; i < 4; ++i) m_ColorAttachments[i] = 0;

		glDeleteTextures(1, &m_DepthAttachment);
		m_DepthAttachment = 0;

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
		uint32_t n = std::clamp(m_Spec.NumColorAttachments, 1u, 4u);
		glDeleteTextures(n, m_ColorAttachments);
		glDeleteTextures(1, &m_DepthAttachment);
		glDeleteFramebuffers(1, &m_RendererID);
	}

}
