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

	OpenGLTexture2D::OpenGLTexture2D(const uint8_t* compressedData, uint32_t byteSize)
	{
		int width, height, channels;
		// GLB embeds textures with top-left origin matching OpenGL's expectation — do NOT flip.
		stbi_set_flip_vertically_on_load(0);
		stbi_uc* data = stbi_load_from_memory(compressedData, (int)byteSize, &width, &height, &channels, 0);
		stbi_set_flip_vertically_on_load(1); // restore for file-based loads

		if (!data || width <= 0 || height <= 0)
		{
			HMN_CORE_WARN("OpenGLTexture2D: embedded texture decode failed ({}x{}) — using fallback", width, height);
			m_Width = m_Height = 1;
			m_InternalFormat = GL_RGBA8; m_DataFormat = GL_RGBA;
			m_MipLevels = 1;
			uint32_t pink = 0xFF00FFFF;
			m_PendingPixels.assign(reinterpret_cast<uint8_t*>(&pink),
			                       reinterpret_cast<uint8_t*>(&pink) + 4);
			if (data) stbi_image_free(data);
			return; // factory calls QueueUpload() after the IntrusiveRef is established
		}

		m_Width  = (uint32_t)width;
		m_Height = (uint32_t)height;

		if      (channels == 4) { m_InternalFormat = GL_RGBA8; m_DataFormat = GL_RGBA; }
		else if (channels == 3) { m_InternalFormat = GL_RGB8;  m_DataFormat = GL_RGB;  }
		else if (channels == 2) { m_InternalFormat = GL_RG8;   m_DataFormat = GL_RG;   }
		else if (channels == 1) { m_InternalFormat = GL_R8;    m_DataFormat = GL_RED;  }

		if (m_InternalFormat == 0)
		{
			HMN_CORE_WARN("OpenGLTexture2D: unsupported channel count {} in embedded texture — skipping", channels);
			stbi_image_free(data);
			m_Width = m_Height = 1; m_InternalFormat = GL_RGBA8; m_DataFormat = GL_RGBA; m_MipLevels = 1;
			uint32_t pink = 0xFF00FFFF; m_PendingPixels.assign((uint8_t*)&pink, (uint8_t*)&pink + 4);
			return; // factory queues the upload
		}

		m_MipLevels = 1 + (int)std::floor(std::log2((float)std::max(m_Width, m_Height)));

		size_t bytes = (size_t)width * height * channels;
		m_PendingPixels.assign(data, data + bytes);
		stbi_image_free(data);
		// factory calls QueueUpload() after the IntrusiveRef is established
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

		if      (channels == 4) { m_InternalFormat = GL_RGBA8; m_DataFormat = GL_RGBA; }
		else if (channels == 3) { m_InternalFormat = GL_RGB8;  m_DataFormat = GL_RGB;  }
		else if (channels == 2) { m_InternalFormat = GL_RG8;   m_DataFormat = GL_RG;   }
		else if (channels == 1) { m_InternalFormat = GL_R8;    m_DataFormat = GL_RED;  }

		if (m_InternalFormat == 0)
		{
			HMN_CORE_WARN("OpenGLTexture2D: unsupported channel count {} in '{}' — skipping", channels, path);
			stbi_image_free(data);
			return;
		}

		m_MipLevels = 1 + (int)std::floor(std::log2((float)std::max(m_Width, m_Height)));

		size_t bytes = (size_t)width * height * channels;
		m_PendingPixels.assign(data, data + bytes);
		stbi_image_free(data);
		// factory calls QueueUpload() after the IntrusiveRef is established
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
		if (m_InternalFormat == 0 || m_MipLevels < 1 || m_Width == 0 || m_Height == 0)
		{
			HMN_CORE_ERROR("UploadToGPU: skipping invalid texture state ({}x{} fmt=0x{:X} mips={} path='{}')",
				m_Width, m_Height, m_InternalFormat, m_MipLevels, m_Path);
			return;
		}
		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, m_MipLevels, m_InternalFormat, m_Width, m_Height);

		GLenum minFilter = (m_MipLevels > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, minFilter);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, WrapToGL(m_WrapS));
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, WrapToGL(m_WrapT));

		if (!m_PendingPixels.empty())
		{
			// Our pixel data is always tightly packed; override the default alignment of 4
			// which would cause glTextureSubImage2D to read past the buffer for RGB/RG/RED formats.
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height,
			                    m_DataFormat, GL_UNSIGNED_BYTE, m_PendingPixels.data());
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // restore default
			if (m_MipLevels > 1)
				glGenerateTextureMipmap(m_RendererID);
		}

		m_PendingPixels.clear();
		m_PendingPixels.shrink_to_fit();
	}


	void OpenGLTexture2D::QueueUpload()
	{
		// Ref constructed from this — keeps object alive until the upload lambda destructs it.
		Ref<Texture2D> self(static_cast<Texture2D*>(this));
		RenderThread::QueueUpload([self = std::move(self)] {
			static_cast<OpenGLTexture2D*>(self.get())->UploadToGPU();
		});
	}

	void OpenGLTexture2D::SetWrapS(TextureWrap wrap)
	{
		m_WrapS = wrap;
		if (m_RendererID)
		{
			uint32_t id = m_RendererID;
			RenderThread::QueueUpload([id, gl = WrapToGL(wrap)] {
				glTextureParameteri(id, GL_TEXTURE_WRAP_S, gl);
			});
		}
	}

	void OpenGLTexture2D::SetWrapT(TextureWrap wrap)
	{
		m_WrapT = wrap;
		if (m_RendererID)
		{
			uint32_t id = m_RendererID;
			RenderThread::QueueUpload([id, gl = WrapToGL(wrap)] {
				glTextureParameteri(id, GL_TEXTURE_WRAP_T, gl);
			});
		}
	}

	void OpenGLTexture2D::SetData(const void* data, uint32_t size)
	{
		uint32_t bytesPerPixel;

		if      (m_DataFormat == GL_RGBA) bytesPerPixel = 4;
		else if (m_DataFormat == GL_RGB)  bytesPerPixel = 3;
		else if (m_DataFormat == GL_RG)   bytesPerPixel = 2;
		else if (m_DataFormat == GL_RED)  bytesPerPixel = 1;
		else { HMN_CORE_ASSERT(false, "Unsupported texture format!"); return; }

		HMN_CORE_ASSERT(size == m_Width * m_Height * bytesPerPixel, "Data must fill the entire texture!");

		const auto* bytes = static_cast<const uint8_t*>(data);
		m_PendingPixels.assign(bytes, bytes + size);

		if (m_RendererID != 0)
		{
			// Move pixel data into the lambda — no need to keep the object alive.
			uint32_t id = m_RendererID, w = m_Width, h = m_Height;
			GLenum fmt = m_DataFormat;
			std::vector<uint8_t> pixels = std::move(m_PendingPixels);
			RenderThread::QueueUpload([id, w, h, fmt, pixels = std::move(pixels)] {
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glTextureSubImage2D(id, 0, 0, 0, w, h, fmt, GL_UNSIGNED_BYTE, pixels.data());
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
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
		if (!m_RendererID) return;
		if (RenderThread::IsOnRenderThread())
		{
			glDeleteTextures(1, &m_RendererID);
		}
		else
		{
			// Destructor called from main thread — queue deletion so it runs on the
			// thread that owns the GL context.
			uint32_t id = m_RendererID;
			RenderThread::QueueUpload([id] { glDeleteTextures(1, &id); });
		}
	}
}