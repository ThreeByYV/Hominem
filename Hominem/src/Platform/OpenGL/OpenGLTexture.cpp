#include "hmnpch.h"
#include "OpenGLTexture.h"
#include "stb_image.h"
#include <glad/glad.h>
#include "Hominem/Renderer/RenderThread.h"

#define ASSERT_RENDER_THREAD() Hominem::RenderThread::AssertRenderThread(__func__)

namespace Hominem {

	// CPU-only — no GL. Caller must call SetData() before first Bind().
	OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height, TextureFormat format)
		: m_Width(width), m_Height(height)
	{
		switch (format)
		{
		case TextureFormat::RGB8:  m_InternalFormat = GL_RGB8;  m_DataFormat = GL_RGB;  break;
		case TextureFormat::RGBA8: m_InternalFormat = GL_RGBA8; m_DataFormat = GL_RGBA; break;
		case TextureFormat::RED8:  m_InternalFormat = GL_R8;    m_DataFormat = GL_RED;  break;
		default: HMN_CORE_ASSERT(false, "Unsupported texture format!"); break;
		}
		// GPU upload deferred to UploadToGPU(), triggered lazily from Bind().
		// SetData() stores pixels into m_PendingPixels until then.
	}

	OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
		: m_Path(path)
	{
		// CPU-only load — no GL calls so this is safe on any thread.
		// The actual GPU upload happens lazily in Bind() on the render thread.
		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);
		stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
		HMN_CORE_ASSERT(data, "Failed to load image!");

		m_Width  = (uint32_t)width;
		m_Height = (uint32_t)height;

		if (channels == 4)      { m_InternalFormat = GL_RGBA8; m_DataFormat = GL_RGBA; }
		else if (channels == 3) { m_InternalFormat = GL_RGB8;  m_DataFormat = GL_RGB;  }
		else if (channels == 1) { m_InternalFormat = GL_R8;    m_DataFormat = GL_RED;  }

		HMN_CORE_ASSERT(m_InternalFormat != 0, "Image format not supported!");

		m_MipLevels = 1 + (int)std::floor(std::log2((float)std::max(m_Width, m_Height)));

		size_t bytes = (size_t)width * height * channels;
		m_PendingPixels.assign(data, data + bytes);
		stbi_image_free(data);
	}

	void OpenGLTexture2D::UploadToGPU() const
	{
		ASSERT_RENDER_THREAD();
		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, m_MipLevels, m_InternalFormat, m_Width, m_Height);

		GLenum minFilter = (m_MipLevels > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, minFilter);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

		if (!m_PendingPixels.empty())
		{
			glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height,
			                    m_DataFormat, GL_UNSIGNED_BYTE, m_PendingPixels.data());
			if (m_MipLevels > 1)
				glGenerateTextureMipmap(m_RendererID);
		}

		m_PendingPixels.clear();
		m_PendingPixels.shrink_to_fit();
	}


	static GLenum WrapToGL(TextureWrap w)
	{
		switch (w)
		{
			case TextureWrap::ClampToEdge:    return GL_CLAMP_TO_EDGE;
			case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
			default:                          return GL_REPEAT;
		}
	}

	void OpenGLTexture2D::SetWrapS(TextureWrap wrap)
	{
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, WrapToGL(wrap));
	}

	void OpenGLTexture2D::SetWrapT(TextureWrap wrap)
	{
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, WrapToGL(wrap));
	}

	void OpenGLTexture2D::SetData(void* data, uint32_t size)
	{
		uint32_t bytesPerPixel;

		if (m_DataFormat == GL_RGBA)
			bytesPerPixel = 4;
		else if (m_DataFormat == GL_RGB)
			bytesPerPixel = 3;
		else if (m_DataFormat == GL_RED)
			bytesPerPixel = 1;
		else
		{
			HMN_CORE_ASSERT(false, "Unsupported texture format!");
			return;
		}

		uint32_t expectedSize = m_Width * m_Height * bytesPerPixel;

		if (size != expectedSize)
		{
			HMN_CORE_ERROR("Data size mismatch! Expected: {} bytes, Got: {} bytes. Width: {}, Height: {}, BPP: {}, Format: {}",
				expectedSize, size, m_Width, m_Height, bytesPerPixel, m_DataFormat);
		}

		HMN_CORE_ASSERT(size == expectedSize, "Data must be enough to fill the entire texture!");

		if (m_RendererID == 0)
		{
			// GPU not yet created — store for lazy upload on first Bind().
			m_PendingPixels.assign(static_cast<uint8_t*>(data),
			                       static_cast<uint8_t*>(data) + size);
		}
		else
		{
			glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);
		}
	}

	void OpenGLTexture2D::Bind(uint32_t slot) const
	{
		if (!m_PendingPixels.empty())
			UploadToGPU();
		glBindTextureUnit(slot, m_RendererID);
	}

	void OpenGLTexture2D::UnbindAll()
	{
		glBindTexture(GL_TEXTURE_2D, 0);

	}

	OpenGLTexture2D::~OpenGLTexture2D()
	{
		if (m_RendererID)
		{
			ASSERT_RENDER_THREAD();
			glDeleteTextures(1, &m_RendererID);
		}
		// If m_RendererID == 0 the texture was never uploaded (destroyed before
		// its first Bind), so no GL object to clean up.
	}
}