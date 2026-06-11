#pragma once

#include "Hominem/Renderer/GBuffer.h"

namespace Hominem {

	class OpenGLGBuffer : public GBuffer
	{
	public:
		OpenGLGBuffer(uint32_t width, uint32_t height);
		~OpenGLGBuffer();

		void BindForWriting() override;

		void BindForReading() override;

		void SetReadBuffer(GBUFFER_TEXTURE_TYPE textureType) override;

		void BlitToScreen(uint32_t screenW, uint32_t screenH) override;

	private:
		uint32_t m_FBO;
		uint32_t m_Textures[GBUFFER_NUM_TEXTURES];
		uint32_t m_DepthTexture;
		uint32_t m_Width, m_Height;
	};

}
