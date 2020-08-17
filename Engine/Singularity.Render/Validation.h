#pragma once
#include <vulkan/vulkan.hpp>

#include <Singularity.Core/CoreDeclare.h>

namespace Singularity
{
	namespace Render
	{
		class Renderer;

		class Validation
		{
		public:
			Validation(Renderer const& _renderer);

			void Initialize();
			void Shutdown();

			bool UseValidationLayers() const; // TODO - VALIDATION.h
			bool ValidationLayersSupported() const;
			VkDebugUtilsMessengerCreateInfoEXT GetDebugUtilsCreateInfo() const;
			std::vector<const char*> const& GetValidationLayers() const { return m_validationLayers; }

		private:
			void SetupValidationLayers();

			VkDebugUtilsMessengerEXT m_debugMessenger;

			Renderer const& m_renderer;

			std::vector<const char*> const m_validationLayers = {
				"VK_LAYER_KHRONOS_validation"
			};

		};
	}
}
