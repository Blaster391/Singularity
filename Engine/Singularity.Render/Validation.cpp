#include "Validation.h"

#include <iostream>

#include <Singularity.Render/Renderer.h>

namespace Singularity
{
	namespace Render
	{
		//////////////////////////////////////////////////////////////////////////////////////
		static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationCallback
		(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData
		) {

			std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

			return VK_FALSE;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
			auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (func != nullptr) {
				return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
			}
			else {
				return VK_ERROR_EXTENSION_NOT_PRESENT;
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
			auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
			if (func != nullptr) {
				func(instance, debugMessenger, pAllocator);
			}
		}

		Validation::Validation(Renderer const& _renderer)
			: m_renderer(_renderer)
		{
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Validation::Initialize()
		{
			SetupValidationLayers();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Validation::Shutdown()
		{
			if (UseValidationLayers()) {
				DestroyDebugUtilsMessengerEXT(m_renderer.GetInstance(), m_debugMessenger, nullptr);
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		bool Validation::UseValidationLayers() const
		{
#if NDEBUG
			return false;
#else
			return true;
#endif
		}

		//////////////////////////////////////////////////////////////////////////////////////
		bool Validation::ValidationLayersSupported() const
		{
			uint32_t layerCount;
			vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

			std::vector<VkLayerProperties> availableLayers(layerCount);
			vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

			for (const char* layerName : m_validationLayers) {
				bool layerFound = false;

				for (const auto& layerProperties : availableLayers) {
					if (strcmp(layerName, layerProperties.layerName) == 0) {
						layerFound = true;
						break;
					}
				}

				if (!layerFound) {
					return false;
				}
			}

			return true;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkDebugUtilsMessengerCreateInfoEXT Validation::GetDebugUtilsCreateInfo() const
		{
			VkDebugUtilsMessengerCreateInfoEXT createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			createInfo.pfnUserCallback = ValidationCallback;
			createInfo.pUserData = nullptr;
			return createInfo;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Validation::SetupValidationLayers()
		{
			if (!UseValidationLayers())
			{
				return;
			}

			VkDebugUtilsMessengerCreateInfoEXT createInfo = GetDebugUtilsCreateInfo();

			if (CreateDebugUtilsMessengerEXT(m_renderer.GetInstance(), &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
				throw std::runtime_error("failed to set up debug messenger!");
			}
		}
	}
}
