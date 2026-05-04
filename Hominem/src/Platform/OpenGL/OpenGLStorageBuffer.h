#pragma once

#include "Hominem/Renderer/StorageBuffer.h"

namespace Hominem {

	class OpenGLStorageBuffer : public StorageBuffer
	{
	public:
		OpenGLStorageBuffer(uint32_t capacity);
		~OpenGLStorageBuffer() override;

		void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;
		void BindBase(uint32_t slot) const override;

	private:
		uint32_t m_RendererID = 0;
	};

	class OpenGLDrawIndirectBuffer : public DrawIndirectBuffer
	{
	public:
		OpenGLDrawIndirectBuffer(uint32_t capacity);
		~OpenGLDrawIndirectBuffer() override;

		void SetData(const void* data, uint32_t size) override;
		void Bind()   const override;
		void Unbind() const override;

	private:
		uint32_t m_RendererID = 0;
	};

}
