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

		 void AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) override;
		 void SetIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) override;
	
		 const std::vector<std::shared_ptr<VertexBuffer>>& GetVertexBuffers() const { return m_VertexBuffers; }
		 const std::shared_ptr<IndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }


	private:
		std::vector<std::shared_ptr<VertexBuffer>> m_VertexBuffers;
		std::shared_ptr<IndexBuffer> m_IndexBuffer;
		unsigned int m_RendererID;
	};
}
