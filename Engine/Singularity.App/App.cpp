#include "App.h"
namespace Singularity
{
	App::App() :
		m_renderer(m_window)
	{
		Initialize();
	}

	App::~App()
	{
		Shutdown();
	}

	void App::Run()
	{
		float const timeStep = 1.0f / 60.0f;
		while (m_window.IsActive())
		{
			m_window.Update(timeStep);
			m_renderer.Update(timeStep);
		}
	}
	
	void App::Initialize()
	{
	}

	void App::Shutdown()
	{
	}
}