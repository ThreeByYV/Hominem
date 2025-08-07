#include "hmnpch.h"
//#include "Window.h"
//
//
//Hominem::Window::Window()
//{
//}
//
//void Hominem::Window::createCallbacks()
//{
//	glfwSetKeyCallback(m_mainWindow, handleKeys);
//	glfwSetCursorCallback(m_mainWindow, handleKeys);
//}
//
//void Hominem::Window::handleKeys(GLFWwindow* window, int key, int code, int action, int mode)
//{
//	Window* theWindow = (Window*)glfwGetWindowUserPointer(window));
//
//	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
//	{
//		glfwSetWindowShouldClose(window, GL_TRUE);
//	}
//
//	if (key >= 0 && key < 1024)
//	{
//		if (action == GLFW_PRESS)
//		{
//			theWindow->m_keys[key] = true;
//		}
//		else if (action == GLFW_RELEASE)
//		{
//			theWindow->m_keys[key] = false;
//		}
//
//		if (key == GLFW_KEY_R)
//		{
//
//		}
//	}
//}
//
//void Hominem::Window::handleMouse(GLFWwindow* window, double xPos, double yPos)
//{
//	Window* theWindow = (Window*)glfwGetWindowUserPointer(window));
//	
//	if (theWindow->m_mouseFirstMove)
//	{
//		theWindow->m_lastX = xPos;
//		theWindow->m_lastY = yPos;
//		theWindow->m_mouseFirstMove = false;
//	}
//
//	theWindow->m_xChange = xPos - theWindow->m_lastX;
//	theWindow->m_yChange = yPos - theWindow->m_lastY;
//
//	theWindow->m_lastX = xPos;
//	theWindow->m_lastY = yPos;
//
//	printf("x:%.6f, y:%.6f\n", theWindow->m_xChange, theWindow->m_yChange);
//
//}
//
//
//Hominem::Window::~Window()
//{
//	glfwDestroyWindow(mainWindow);
//	glfwTerminate();
//}
//
