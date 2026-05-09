#include "hmnpch.h"
#include "OpenGLGBuffer.h"
#include <glad/glad.h>

namespace Hominem {

	OpenGLGBuffer::OpenGLGBuffer(uint32_t width, uint32_t height)
		: m_Width(width), m_Height(height)
	{
		glGenFramebuffers(1, &m_FBO);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);

		// Create the GBuffer textures
		glGenTextures(GBUFFER_NUM_TEXTURES, m_Textures);
		glGenTextures(1, &m_DepthTexture);

		for (uint32_t i = 0; i < GBUFFER_NUM_TEXTURES; i++)
		{
			glBindTexture(GL_TEXTURE_2D, m_Textures[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
			
			// GL default min filter requires mipmaps — set to NEAREST to keep FBO complete
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, m_Textures[i], 0);
		}

		// Depth
		glBindTexture(GL_TEXTURE_2D, m_DepthTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DepthTexture, 0);

		GLenum drawBuffers[GBUFFER_NUM_TEXTURES];
	
		for (uint32_t i = 0; i < GBUFFER_NUM_TEXTURES; i++)
		{
			drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
		}
	
		// Enable MRT — one draw buffer per GBuffer attachment
		glDrawBuffers(GBUFFER_NUM_TEXTURES, drawBuffers);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
			HMN_CORE_ERROR("OpenGLGBuffer: framebuffer incomplete, status: {0}", status);

		// Restore default FBO
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	}

	OpenGLGBuffer::~OpenGLGBuffer()
	{
		glDeleteTextures(GBUFFER_NUM_TEXTURES, m_Textures);
		glDeleteTextures(1, &m_DepthTexture);
		glDeleteFramebuffers(1, &m_FBO);
	}

	void OpenGLGBuffer::BindForWriting()
	{
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);
	}

	void OpenGLGBuffer::BindForReading()
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO);
	}

	void OpenGLGBuffer::SetReadBuffer(GBUFFER_TEXTURE_TYPE textureType)
	{
		glReadBuffer(GL_COLOR_ATTACHMENT0 + textureType);
	}

	void OpenGLGBuffer::BlitToScreen(uint32_t screenW, uint32_t screenH)
	{
		// Default FBO as draw, GBuffer as read
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO);

		const uint32_t hw = screenW / 2;
		const uint32_t hh = screenH / 2;

		// Tile all 4 attachments into screen quadrants:
		//   bottom-left  = Position
		//   bottom-right = Diffuse
		//   top-left     = Normal
		//   top-right    = TexCoord
		glReadBuffer(GL_COLOR_ATTACHMENT0 + GBUFFER_TEXTURE_TYPE_POSITION);
		glBlitFramebuffer(0, 0, m_Width, m_Height,   0,  0, hw, hh, GL_COLOR_BUFFER_BIT, GL_LINEAR);

		glReadBuffer(GL_COLOR_ATTACHMENT0 + GBUFFER_TEXTURE_TYPE_DIFFUSE);
		glBlitFramebuffer(0, 0, m_Width, m_Height,  hw,  0, screenW, hh, GL_COLOR_BUFFER_BIT, GL_LINEAR);

		glReadBuffer(GL_COLOR_ATTACHMENT0 + GBUFFER_TEXTURE_TYPE_NORMAL);
		glBlitFramebuffer(0, 0, m_Width, m_Height,   0, hh, hw, screenH, GL_COLOR_BUFFER_BIT, GL_LINEAR);

		glReadBuffer(GL_COLOR_ATTACHMENT0 + GBUFFER_TEXTURE_TYPE_TEXCOORD);
		glBlitFramebuffer(0, 0, m_Width, m_Height,  hw, hh, screenW, screenH, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	}

}
