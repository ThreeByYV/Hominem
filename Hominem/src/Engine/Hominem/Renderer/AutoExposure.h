#pragma once

#include "Hominem/Renderer/Shader.h"

namespace Hominem {

	class AutoExposure
	{
	public:
		~AutoExposure();

		void Init(Ref<ComputeShader> shader);
		void Compute(uint32_t hdrTextureId, uint32_t width, uint32_t height);

		float GetExposure() const { return m_Exposure; }

	private:
		Ref<ComputeShader>  m_Shader;
		uint32_t            m_SSBO       = 0;
		int                 m_SSBOTiles  = 0;
		int                 m_NumGroupsX = 0;
		int                 m_NumGroupsY = 0;
		float               m_Exposure   = 1.0f;
		std::vector<float>  m_ReadBuffer;
	};

}
