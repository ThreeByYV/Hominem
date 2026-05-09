#include "hmnpch.h"
#include "OpenGLTexture.h"
#include "stb_image.h"
#include <glad/glad.h>
#include "Hominem/Renderer/RenderThread.h"

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

		RenderThread::QueueUpload([this] { UploadToGPU(); });
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

	void OpenGLTexture2D::UploadToGPU() const
	{
		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, m_MipLevels, m_InternalFormat, m_Width, m_Height);

		GLenum minFilter = (m_MipLevels > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, minFilter);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, WrapToGL(m_WrapS));
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, WrapToGL(m_WrapT));

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


	void OpenGLTexture2D::SetWrapS(TextureWrap wrap)
	{
		m_WrapS = wrap;
		if (m_RendererID)
			RenderThread::QueueUpload([this, gl = WrapToGL(wrap)] {
				glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, gl);
			});
	}

	void OpenGLTexture2D::SetWrapT(TextureWrap wrap)
	{
		m_WrapT = wrap;
		if (m_RendererID)
			RenderThread::QueueUpload([this, gl = WrapToGL(wrap)] {
				glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, gl);
			});
	}

	void OpenGLTexture2D::SetData(const void* data, uint32_t size)
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

		const auto* bytes = static_cast<const uint8_t*>(data);
		bool firstQueue = m_PendingPixels.empty() && m_RendererID == 0;
		m_PendingPixels.assign(bytes, bytes + size);

		if (m_RendererID == 0)
		{
			// If m_PendingPixels was already non-empty, UploadToGPU() is already queued
			// and will pick up the updated pixels. Only queue on the first SetData call.
			if (firstQueue)
				RenderThread::QueueUpload([this] { UploadToGPU(); });
		}
		else
		{
			RenderThread::QueueUpload([this] {
				glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height,
				                    m_DataFormat, GL_UNSIGNED_BYTE, m_PendingPixels.data());
				m_PendingPixels.clear();
				m_PendingPixels.shrink_to_fit();
			});
		}
	}

	void OpenGLTexture2D::Bind(uint32_t slot) const
	{
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
			glDeleteTextures(1, &m_RendererID);
		}
		// If m_RendererID == 0 the texture was never uploaded (destroyed before
		// its first Bind), so no GL object to clean up.
	}
}