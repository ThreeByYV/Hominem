#include "hmnpch.h"
#include "OpenGLStorageBuffer.h"
#include <glad/glad.h>
#include "Hominem/Renderer/RenderThread.h"
#define ASSERT_RENDER_THREAD() Hominem::RenderThread::AssertRenderThread(__func__)

namespace Hominem {

	OpenGLStorageBuffer::OpenGLStorageBuffer(uint32_t capacity)
	{
		ASSERT_RENDER_THREAD();
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, capacity, nullptr, GL_DYNAMIC_DRAW);
	}

	OpenGLStorageBuffer::~OpenGLStorageBuffer()
	{
		ASSERT_RENDER_THREAD();
		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLStorageBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
	{
		glNamedBufferSubData(m_RendererID, offset, size, data);
	}

	void OpenGLStorageBuffer::BindBase(uint32_t slot) const
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, m_RendererID);
	}

	OpenGLDrawIndirectBuffer::OpenGLDrawIndirectBuffer(uint32_t capacity)
	{
		ASSERT_RENDER_THREAD();
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, capacity, nullptr, GL_DYNAMIC_DRAW);
	}

	OpenGLDrawIndirectBuffer::~OpenGLDrawIndirectBuffer()
	{
		ASSERT_RENDER_THREAD();
		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLDrawIndirectBuffer::SetData(const void* data, uint32_t size)
	{
		glNamedBufferSubData(m_RendererID, 0, size, data);
	}

	void OpenGLDrawIndirectBuffer::Bind() const
	{
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_RendererID);
	}

	void OpenGLDrawIndirectBuffer::Unbind() const
	{
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	}

}
