#include "hmnpch.h"
#include "OpenGLRendererAPI.h"

#include <glad/glad.h>

namespace Hominem {

	void OpenGLRendererAPI::Init()
	{
#ifdef HMN_DEBUG
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // callback fires on the offending thread before the call returns — allows stack traces in debugger

		glDebugMessageCallback([](GLenum source, GLenum type, GLuint id,
		                          GLenum severity, GLsizei, const GLchar* message,
		                          const void*)
		{
			// Suppress noisy NVIDIA driver bookkeeping messages that carry no actionable info
			if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;
			if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
			if (id == 2) return; // API_ID_RECOMPILE_FRAGMENT_SHADER — one-time per format pair

			auto srcStr = [source]() -> const char* {
				switch (source) {
					case GL_DEBUG_SOURCE_API:             return "API";
					case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   return "WindowSystem";
					case GL_DEBUG_SOURCE_SHADER_COMPILER: return "ShaderCompiler";
					case GL_DEBUG_SOURCE_THIRD_PARTY:     return "ThirdParty";
					case GL_DEBUG_SOURCE_APPLICATION:     return "App";
					default:                              return "Other";
				}
			}();

			auto typeStr = [type]() -> const char* {
				switch (type) {
					case GL_DEBUG_TYPE_ERROR:               return "Error";
					case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Deprecated";
					case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  return "UndefinedBehaviour";
					case GL_DEBUG_TYPE_PORTABILITY:         return "Portability";
					case GL_DEBUG_TYPE_PERFORMANCE:         return "Performance";
					case GL_DEBUG_TYPE_MARKER:              return "Marker";
					default:                                return "Other";
				}
			}();

			// Map the id to a GL error enum name when the driver passes one as the message ID
			auto glErrName = [id]() -> const char* {
				switch (id) {
					case 0x0500: return "GL_INVALID_ENUM";
					case 0x0501: return "GL_INVALID_VALUE";
					case 0x0502: return "GL_INVALID_OPERATION";
					case 0x0503: return "GL_STACK_OVERFLOW";
					case 0x0504: return "GL_STACK_UNDERFLOW";
					case 0x0505: return "GL_OUT_OF_MEMORY";
					case 0x0506: return "GL_INVALID_FRAMEBUFFER_OPERATION";
				case 0x0507: return "GL_CONTEXT_LOST";
					default:     return nullptr;
				}
			}();

			// Build a single id string so all severity branches share one format.
			char idBuf[64];
			if (glErrName)
				snprintf(idBuf, sizeof(idBuf), "%u (%s)", id, glErrName);
			else
				snprintf(idBuf, sizeof(idBuf), "%u", id);

			switch (severity)
			{
				case GL_DEBUG_SEVERITY_HIGH:
					HMN_CORE_ERROR("[OpenGL] ({}) {} id={}: {}", srcStr, typeStr, idBuf, message);
					HMN_CORE_ASSERT(false, "OpenGL high-severity error — see log above");
					break;
				case GL_DEBUG_SEVERITY_MEDIUM:
					HMN_CORE_WARN("[OpenGL] ({}) {} id={}: {}", srcStr, typeStr, idBuf, message);
					break;
				case GL_DEBUG_SEVERITY_LOW:
					HMN_CORE_INFO("[OpenGL] ({}) {} id={}: {}", srcStr, typeStr, idBuf, message);
					break;
			}
		}, nullptr);

		// Silence low-value performance hints globally; re-enable per-investigation if needed
		glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_PERFORMANCE, GL_DEBUG_SEVERITY_LOW, 0, nullptr, GL_FALSE);
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

	void OpenGLRendererAPI::SetScissorEnabled(bool enabled)
	{
		if (enabled) glEnable(GL_SCISSOR_TEST);
		else         glDisable(GL_SCISSOR_TEST);
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

	void OpenGLRendererAPI::DrawFullscreenTriangle()
	{
		glBindVertexArray(m_EmptyVAO);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	void OpenGLRendererAPI::DrawPoints(uint32_t count, float pointSize)
	{
		glPointSize(pointSize);
		glDrawArrays(GL_POINTS, 0, (GLsizei)count);
		glPointSize(1.f);
	}

	void OpenGLRendererAPI::DrawPatches(uint32_t count, uint32_t patchVertices)
	{
		glPatchParameteri(GL_PATCH_VERTICES, (GLint)patchVertices);
		glDrawArrays(GL_PATCHES, 0, (GLsizei)count);
	}

	void OpenGLRendererAPI::SetWireframe(bool enabled)
	{
		glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
	}

	void OpenGLRendererAPI::BindTexture(uint32_t slot, uint32_t id)
	{
		glBindTextureUnit(slot, id);
	}

	const char* OpenGLRendererAPI::GetGPUVendor() const
	{
		return (const char*)glGetString(GL_VENDOR);
	}

	const char* OpenGLRendererAPI::GetGPURenderer() const
	{
		return (const char*)glGetString(GL_RENDERER);
	}

}