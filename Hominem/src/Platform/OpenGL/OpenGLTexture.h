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

		virtual ~OpenGLTexture2D();

		uint32_t GetWidth() const override { return m_Width; }
		uint32_t GetHeight() const override { return m_Height; }

		void SetData(void* data, uint32_t size) override;
		void Bind(uint32_t slot) const override;
		void SetWrapS(TextureWrap wrap) override;
		void SetWrapT(TextureWrap wrap) override;

		static void UnbindAll();

	private:
		void UploadToGPU() const; // called lazily from Bind() on the render thread

		std::string m_Path;
		uint32_t m_Width = 0, m_Height = 0;
		mutable uint32_t m_RendererID = 0;
		GLenum m_InternalFormat = 0, m_DataFormat = 0;

		// CPU pixel data held until first Bind() on the render thread
		mutable std::vector<uint8_t> m_PendingPixels;
		mutable int m_MipLevels = 1;
	};
}

