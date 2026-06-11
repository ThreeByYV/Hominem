#include "hmnpch.h"
#include "AutoExposure.h"

#include <glad/glad.h>
#include <cmath>
#include <algorithm>

namespace Hominem {

	AutoExposure::~AutoExposure()
	{
		if (m_SSBO)
			glDeleteBuffers(1, &m_SSBO);
	}

	void AutoExposure::Init(Ref<ComputeShader> shader)
	{
		m_Shader = std::move(shader);
		glCreateBuffers(1, &m_SSBO);
	}

	void AutoExposure::Compute(uint32_t hdrTextureId, uint32_t width, uint32_t height)
	{
		if (!m_Shader || width == 0 || height == 0) return;

		m_NumGroupsX    = (int)((width  + 9) / 10);
		m_NumGroupsY    = (int)((height + 9) / 10);
		int numTiles    = m_NumGroupsX * m_NumGroupsY;

		if (numTiles > m_SSBOTiles)
		{
			if (m_SSBO) glDeleteBuffers(1, &m_SSBO);
			glCreateBuffers(1, &m_SSBO);
			glNamedBufferData(m_SSBO, numTiles * sizeof(float), nullptr, GL_STREAM_READ);
			m_SSBOTiles  = numTiles;
			m_ReadBuffer.resize(numTiles);
		}

		m_Shader->Bind();
		glBindTextureUnit(0, hdrTextureId);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_SSBO);
		m_Shader->Dispatch(m_NumGroupsX, m_NumGroupsY, 1);
		// Barrier ensures compute writes are visible before the CPU readback below.
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glGetNamedBufferSubData(m_SSBO, 0, numTiles * sizeof(float), m_ReadBuffer.data());

		float sum = 0.0f;
		for (int i = 0; i < numTiles; ++i)
			sum += m_ReadBuffer[i];

		float avgLogLum = sum / (float)(numTiles * 100);
		float avgLum    = expf(avgLogLum);
		m_Exposure      = std::clamp(0.18f / avgLum, 0.05f, 20.0f);
	}

}
