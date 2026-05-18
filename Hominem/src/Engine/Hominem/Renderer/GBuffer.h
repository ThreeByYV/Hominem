#pragma once

#include "Hominem/Core/Core.h"

namespace Hominem {

	class GBuffer : public RefCounted
	{
	public:
		enum GBUFFER_TEXTURE_TYPE
		{
			GBUFFER_TEXTURE_TYPE_POSITION = 0,
			GBUFFER_TEXTURE_TYPE_DIFFUSE,
			GBUFFER_TEXTURE_TYPE_NORMAL,
			GBUFFER_TEXTURE_TYPE_TEXCOORD,
			GBUFFER_NUM_TEXTURES,
		};

		static Ref<GBuffer> Create(uint32_t width, uint32_t height);

		// Binds the GBuffer FBO as the draw target (geometry pass)
		virtual void BindForWriting() = 0;

		// Binds the GBuffer FBO as the read source (light pass)
		virtual void BindForReading() = 0;

		virtual void SetReadBuffer(GBUFFER_TEXTURE_TYPE textureType) = 0;

		// Debug: binds default FBO as draw, self as read, blits all 4 attachments into screen quadrants
		virtual void BlitToScreen(uint32_t screenW, uint32_t screenH) = 0;

		virtual ~GBuffer() = default;
	};

}
