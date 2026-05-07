#include "hmnpch.h"
#include "OpenGLRendererAPI.h"

#include <glad/glad.h>
#include "Hominem/Renderer/RenderThread.h"
#define ASSERT_RENDER_THREAD() Hominem::RenderThread::AssertRenderThread(__func__)

namespace Hominem {

	void OpenGLRendererAPI::Init()
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glEnable(GL_DEPTH_TEST);

		glCreateVertexArrays(1, &m_EmptyVAO);
	}

	void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
	{
		ASSERT_RENDER_THREAD();
		glClearColor(color.r, color.g, color.b, color.a);
	}

	void OpenGLRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		glViewport(x, y, width, height); //telling opengl the rendering area has changed, render into this now
	}

	void OpenGLRendererAPI::Clear()
	{
		ASSERT_RENDER_THREAD();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray)
	{
		glDrawElements(GL_TRIANGLES, vertexArray->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);
	}

	void OpenGLRendererAPI::BindEmptyVAO()
	{
		glBindVertexArray(m_EmptyVAO);
	}

	void OpenGLRendererAPI::UnbindVAO()
	{
		glBindVertexArray(0);
	}

	void OpenGLRendererAPI::DrawArraysIndirect(uint32_t offset)
	{
		glDrawArraysIndirect(GL_TRIANGLES, reinterpret_cast<const void*>(static_cast<uintptr_t>(offset)));
	}

}