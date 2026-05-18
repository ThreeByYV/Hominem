#pragma once

#include "Hominem/Renderer/Texture.h"
#include <glad/glad.h>
#include <vector>

namespace Hominem {

	class OpenGLTexture2D : public Texture2D
	{
	public:
		OpenGLTexture2D(uint32_t width, uint32_t height, TextureFormat format = TextureFormat::RGBA8);
		OpenGLTexture2D(const std::string& path);
		OpenGLTexture2D(const uint8_t* compressedData, uint32_t byteSize);

		virtual ~OpenGLTexture2D();

		uint32_t GetWidth() const override { return m_Width; }
		uint32_t GetHeight() const override { return m_Height; }

		void SetData(const void* data, uint32_t size) override;
		void Bind(uint32_t slot) const override;
		void SetWrapS(TextureWrap wrap) override;
		void SetWrapT(TextureWrap wrap) override;

		static void UnbindAll();

		// Called by the factory after the intrusive ref is established.
		void QueueUpload() override;

	private:
		void UploadToGPU() const;

		std::string m_Path;
		uint32_t m_Width = 0, m_Height = 0;
		mutable uint32_t m_RendererID = 0;
		GLenum m_InternalFormat = 0, m_DataFormat = 0;

		// CPU-side state held until first Bind() on the render thread
		mutable std::vector<uint8_t> m_PendingPixels;
		mutable int         m_MipLevels = 1;
		mutable TextureWrap m_WrapS     = TextureWrap::Repeat;
		mutable TextureWrap m_WrapT     = TextureWrap::Repeat;
	};
}

