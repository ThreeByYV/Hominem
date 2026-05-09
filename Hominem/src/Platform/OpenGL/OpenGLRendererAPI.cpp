#include "hmnpch.h"
#include "OpenGLRendererAPI.h"

#include <glad/glad.h>

namespace Hominem {

	void OpenGLRendererAPI::Init()
	{
#ifdef HMN_DEBUG
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // fires on the same thread as the bad call, before it returns

		glDebugMessageCallback([](GLenum source, GLenum type, GLuint id,
		                          GLenum severity, GLsizei, const GLchar* message,
		                          const void*)
		{
			if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

			if (type == GL_DEBUG_TYPE_ERROR)
			{
				HMN_CORE_ERROR("OpenGL error (src={:#x} type={:#x} id={}): {}", source, type, id, message);
				HMN_CORE_ASSERT(false, "OpenGL error — see log above");
			}
			else
			{
				HMN_CORE_WARN("OpenGL warning (src={:#x} type={:#x} id={}): {}", source, type, id, message);
			}
		}, nullptr);
#endif

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glCreateVertexArrays(1, &m_EmptyVAO);
	}

	void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
	{
		glClearColor(color.r, color.g, color.b, color.a);
	}

	void OpenGLRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		glViewport(x, y, width, height); //telling opengl the rendering area has changed, render into this now
	}

	void OpenGLRendererAPI::Clear()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray)
	{
		glDrawElements(GL_TRIANGLES, vertexArray->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);
	}

	void OpenGLRendererAPI::DrawIndexedLines(const Ref<VertexArray>& vertexArray, uint32_t indexCount)
	{
		glDrawElements(GL_LINES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, nullptr);
	}

	void OpenGLRendererAPI::SetDepthTestEnabled(bool enabled)
	{
		if (enabled) glEnable(GL_DEPTH_TEST);
		else         glDisable(GL_DEPTH_TEST);
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