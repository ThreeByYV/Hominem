#include "hmnpch.h"
#include "OpenGLVertexArray.h"
#include "OpenGLBuffer.h"
#include <glad/glad.h>

namespace Hominem {

	static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
	{
		switch (type)
		{
			case ShaderDataType::Float:     return GL_FLOAT;
			case ShaderDataType::Float2:    return GL_FLOAT;
			case ShaderDataType::Float3:    return GL_FLOAT;
			case ShaderDataType::Float4:    return GL_FLOAT;
			case ShaderDataType::Mat3:      return GL_FLOAT;
			case ShaderDataType::Mat4:      return GL_FLOAT;
			case ShaderDataType::Int:       return GL_INT;
			case ShaderDataType::Int2:      return GL_INT;
			case ShaderDataType::Int3:      return GL_INT;
			case ShaderDataType::Int4:      return GL_INT;
			case ShaderDataType::Bool:      return GL_BOOL;
		}

		HMN_CORE_ASSERT(false, "Unknown ShaderDataType!");
		return 0;
	}

	OpenGLVertexArray::OpenGLVertexArray()
	{
		glCreateVertexArrays(1, &m_RendererID);
	}

	void OpenGLVertexArray::Bind() const
	{
		glBindVertexArray(m_RendererID);
	}

	void OpenGLVertexArray::Unbind() const
	{
		glBindVertexArray(0);
	}

	void OpenGLVertexArray::AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer)
	{
		HMN_CORE_ASSERT(!vertexBuffer->GetLayout().GetElements().empty(), "Vertex Buffer has no layout!");

		const auto& layout     = vertexBuffer->GetLayout();
		const uint32_t binding = static_cast<uint32_t>(m_VertexBuffers.size());
		const uint32_t vboID   = static_cast<OpenGLVertexBuffer*>(vertexBuffer.get())->GetRendererID();

		glVertexArrayVertexBuffer(m_RendererID, binding, vboID, 0, layout.GetStride());

		for (const auto& element : layout)
		{
			glEnableVertexArrayAttrib(m_RendererID, m_AttribIndex);
			glVertexArrayAttribFormat(m_RendererID, m_AttribIndex,
				element.GetComponentCount(),
				ShaderDataTypeToOpenGLBaseType(element.Type),
				element.Normalized ? GL_TRUE : GL_FALSE,
				element.Offset);
			glVertexArrayAttribBinding(m_RendererID, m_AttribIndex, binding);
			m_AttribIndex++;
		}

		m_VertexBuffers.push_back(vertexBuffer);
	}

	void OpenGLVertexArray::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer)
	{
		const uint32_t iboID = static_cast<OpenGLIndexBuffer*>(indexBuffer.get())->GetRendererID();
		glVertexArrayElementBuffer(m_RendererID, iboID);
		m_IndexBuffer = indexBuffer;
	}

	OpenGLVertexArray::~OpenGLVertexArray()
	{
		glDeleteVertexArrays(1, &m_RendererID);
	}
}
