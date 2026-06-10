#include "hmnpch.h"
#include "OpenGLStorageBuffer.h"
#include "Hominem/Renderer/RenderThread.h"
#include <glad/glad.h>

namespace Hominem {

	// ---- UniformBuffer ----------------------------------------------------

	OpenGLUniformBuffer::OpenGLUniformBuffer(uint32_t size, uint32_t binding)
	{
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, size, nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_RendererID);
	}

	OpenGLUniformBuffer::~OpenGLUniformBuffer()
	{
		if (!m_RendererID) return;
		if (RenderThread::IsOnRenderThread())
			glDeleteBuffers(1, &m_RendererID);
		else
		{
			uint32_t id = m_RendererID;
			RenderThread::QueueUpload([id] { glDeleteBuffers(1, &id); });
		}
	}

	void OpenGLUniformBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
	{
		glNamedBufferSubData(m_RendererID, offset, size, data);
	}

	Ref<UniformBuffer> UniformBuffer::Create(uint32_t size, uint32_t binding)
	{
		return CreateRef<OpenGLUniformBuffer>(size, binding);
	}

	// -----------------------------------------------------------------------

	OpenGLStorageBuffer::OpenGLStorageBuffer(uint32_t capacity)
	{
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, capacity, nullptr, GL_DYNAMIC_DRAW);
	}

	OpenGLStorageBuffer::~OpenGLStorageBuffer()
	{
		if (!m_RendererID) return;
		if (RenderThread::IsOnRenderThread())
			glDeleteBuffers(1, &m_RendererID);
		else
		{
			uint32_t id = m_RendererID;
			RenderThread::QueueUpload([id] { glDeleteBuffers(1, &id); });
		}
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
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, capacity, nullptr, GL_DYNAMIC_DRAW);
	}

	OpenGLDrawIndirectBuffer::~OpenGLDrawIndirectBuffer()
	{
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
