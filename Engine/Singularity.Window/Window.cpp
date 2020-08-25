#include "Window.h"

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace Singularity
{
	namespace Window
	{
		//////////////////////////////////////////////////////////////////////////////////////
		Window::Window()
		{
			Initialize();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		Window::~Window()
		{
			Shutdown();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Window::Update(float _timeStep)
		{
			if (!m_active)
			{
				return;
			}

			if (glfwWindowShouldClose(m_window))
			{
				m_active = false;
			}
			else
			{
				glfwPollEvents();
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		WindowExtensionsInfo Window::GetExtensions() const
		{
			WindowExtensionsInfo extensions;
			extensions.m_extensions = glfwGetRequiredInstanceExtensions(&extensions.m_extensionCount);
			return extensions;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		HWND Window::GetHandle() const
		{
			return glfwGetWin32Window(m_window);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Window::SetSize(uint32 _width, uint32 _height)
		{
			m_width = _width;
			m_height = _height;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		static void ResizeCallback(GLFWwindow* _window, int _width, int _height)
		{
			Window* window = (Window*)glfwGetWindowUserPointer(_window);
			window->SetSize(_width, _height);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Window::Initialize()
		{
			glfwInit();

			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

			m_window = glfwCreateWindow(m_width, m_height, m_title, nullptr, nullptr);
			glfwSetWindowUserPointer(m_window, this);
			glfwSetFramebufferSizeCallback(m_window, ResizeCallback);

			m_active = true;
		}

		void Window::Shutdown()
		{
			glfwDestroyWindow(m_window);
			glfwTerminate();
		}
	}
}