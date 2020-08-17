#pragma once
#include <optional>
#include <vulkan/vulkan.hpp>

#include <Singularity.Core/CoreDeclare.h>

namespace Singularity
{
	namespace Render
	{
		class Renderer;

		struct QueueFamilies
		{
			std::optional<uint32> m_graphicsFamily;
			std::optional<uint32> m_presentFamily;

			bool IsValid() const
			{
				return m_graphicsFamily.has_value() && m_presentFamily.has_value();
			}
		};

		struct SwapChainSupportDetails {
			VkSurfaceCapabilitiesKHR m_capabilities;
			std::vector<VkSurfaceFormatKHR> m_formats;
			std::vector<VkPresentModeKHR> m_presentModes;
		};

		class Device
		{
		public:
			Device(Renderer const& _renderer);
			virtual ~Device() {}

			void Initialize();
			void Shutdown();

			bool IsReady() const { return m_ready; }

			VkDevice GetLogicalDevice() const { return m_logicalDevice; }
			VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }

			QueueFamilies const& GetQueueFamilies() const { return m_deviceQueueFamilies; }
			SwapChainSupportDetails const& GetSwapChainSupportDetails() const { return m_swapChainSupportDetails; }

			VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
			VkQueue GetPresentQueue() const { return m_presentQueue; }

		private:
			void SelectPhysicalDevice();
			void CreateLogicalDevice();
			void SetDeviceQueues();

			bool IsPhysicalDeviceSuitable(VkPhysicalDevice _device) const;
			bool HasExtensionSupport(VkPhysicalDevice _device) const;
			bool HasSwapChainSupport(VkPhysicalDevice _device) const;
			QueueFamilies FindQueueFamilies(VkPhysicalDevice _device) const;
			SwapChainSupportDetails FindSwapChainSupport(VkPhysicalDevice _device) const;

			VkDeviceQueueCreateInfo GetDeviceQueueCreateInfo(uint32 queueFamily) const;

			Renderer const& m_renderer;

			VkDevice m_logicalDevice = VK_NULL_HANDLE;
			VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
			QueueFamilies m_deviceQueueFamilies;
			SwapChainSupportDetails m_swapChainSupportDetails;
			VkQueue m_graphicsQueue;
			VkQueue m_presentQueue;

			bool m_ready = false;

			std::vector<const char*> const m_deviceExtensions = {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME
			};
		};

	}
}