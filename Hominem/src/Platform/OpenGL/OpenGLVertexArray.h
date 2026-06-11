#pragma once

#include "Hominem/Renderer/VertexArray.h"

namespace Hominem {

	class OpenGLVertexArray : public VertexArray
	{
	public:
		OpenGLVertexArray();
		~OpenGLVertexArray();

		 void Bind() const override;
		 void Unbind() const override;

		 void AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) override;
		 void SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer) override;
	
		 const std::vector<Ref<VertexBuffer>>& GetVertexBuffers() const { return m_VertexBuffers; }
		 const Ref<IndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }


	private:
		std::vector<Ref<VertexBuffer>> m_VertexBuffers;
		Ref<IndexBuffer> m_IndexBuffer;
		uint32_t m_RendererID  = 0;
		uint32_t m_AttribIndex = 0; // increments across multiple AddVertexBuffer calls
	};
}
