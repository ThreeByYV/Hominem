#include "hmnpch.h"
#include "OpenGLBuffer.h"
#include <glad/glad.h>

namespace Hominem {

	//█░█ █▀▀ █▀█ ▀█▀ █▀▀ ▀▄▀   █▄▄ █░█ █▀▀ █▀▀ █▀▀ █▀█
	//▀▄▀ ██▄ █▀▄ ░█░ ██▄ █░█   █▄█ █▄█ █▀░ █▀░ ██▄ █▀▄

	OpenGLVertexBuffer::OpenGLVertexBuffer(float* vertices, uint32_t size)
	{
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, size, vertices, GL_STATIC_DRAW);
	}

	OpenGLVertexBuffer::OpenGLVertexBuffer(uint32_t size)
	{
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, size, nullptr, GL_DYNAMIC_DRAW);
	}

	void OpenGLVertexBuffer::SetData(const void* data, uint32_t size)
	{
		glNamedBufferSubData(m_RendererID, 0, size, data);
	}

	OpenGLVertexBuffer::~OpenGLVertexBuffer()
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	//   █ █▄░█ █▀▄ █▀▀ ▀▄▀   █▄▄ █░█ █▀▀ █▀▀ █▀▀ █▀█
	//   █ █░▀█ █▄▀ ██▄ █░█   █▄█ █▄█ █▀░ █▀░ ██▄ █▀▄

	OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indices, uint32_t count)
		: m_Count(count)
	{
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
	}

	OpenGLIndexBuffer::~OpenGLIndexBuffer()
	{
		glDeleteBuffers(1, &m_RendererID);
	}
}
