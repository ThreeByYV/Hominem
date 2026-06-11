#include "hmnpch.h"
#include "OpenGLContext.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>


namespace Hominem {

	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		:m_WindowHandle(windowHandle)
	{
		HMN_CORE_ASSERT(windowHandle, "Window handle is null!");
	}

	void OpenGLContext::Init()
	{
		glfwMakeContextCurrent(m_WindowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		HMN_CORE_ASSERT(status, "Failed to initialize Glad!");

		HMN_CORE_INFO("GPU Vendor:   {}", (const char*)glGetString(GL_VENDOR));
		HMN_CORE_INFO("GPU Renderer: {}", (const char*)glGetString(GL_RENDERER));
		HMN_CORE_INFO("GL Version:   {}", (const char*)glGetString(GL_VERSION));
		// Context stays current so the caller (WindowsWindow::Init) can call
		// SetVSync immediately after. Application::Run releases it before the
		// render thread starts.
	}

	void OpenGLContext::SwapBuffers()
	{
		glfwSwapBuffers(m_WindowHandle);
	}
}
