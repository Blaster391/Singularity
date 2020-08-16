#pragma once
// External Includes


// Includes
#include <Singularity.Core/CoreDeclare.h>

// Forward Declares
struct GLFWwindow;

namespace Singularity
{
	namespace Window
	{
		struct WindowExtensionsInfo
		{
			uint32_t m_extensionCount = 0u;
			const char** m_extensions = nullptr;
		};

		class Window
		{
		public:
			Window();
			~Window();

			void Update(float _timeStep);
			bool IsActive() const { return m_active; }

			uint32 GetWidth() const { return m_width; }
			uint32 GetHeight() const { return m_height; }

			WindowExtensionsInfo GetExtensions() const;
			HWND GetHandle() const;

		private:
			void Initialize();
			void Shutdown();

			GLFWwindow* m_window = nullptr;
			uint32 m_width = 1200u;
			uint32 m_height = 800u;
			char const* m_title = "Test Window";

			bool m_active = false;
		};
	}
}
