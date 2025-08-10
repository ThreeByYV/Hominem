#pragma once

#include "Hominem/Renderer/GraphicsContext.h"

struct GLFWwindow;

namespace Hominem {
	//will be needed for later when refactoring to use different Graphics API each will have their own context
	
	class OpenGLContext: public GraphicsContext
	{
	public:
		 OpenGLContext(GLFWwindow* windowHandle);
		 
		 void Init() override;
		 void SwapBuffers() override;
	private:
		GLFWwindow* m_WindowHandle;

	};

}

