#pragma once

#include <Singularity.Render/Renderer.h>
#include <Singularity.Window/Window.h>

namespace Singularity
{
	class App
	{
	public:
		App();
		~App();

		void Run();
	private:
		void Initialize();
		void Shutdown();

		Window::Window m_window;
		Render::Renderer m_renderer;
	};
}



