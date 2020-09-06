#include "Device.h"

// External
#include <set>

// Engine
#include <Singularity.Render/Renderer.h>

namespace Singularity
{
	namespace Render
	{
		//////////////////////////////////////////////////////////////////////////////////////
		Device::Device(Renderer const& _renderer)
			:m_renderer(_renderer)
		{
		}
		//////////////////////////////////////////////////////////////////////////////////////
		void Device::Initialize()
		{
			SelectPhysicalDevice();
			CreateLogicalDevice();
			SetDeviceQueues();

			m_ready = true;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Device::Shutdown()
		{
			vkDestroyDevice(m_logicalDevice, nullptr);

			m_ready = false;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Device::RecalculateSwapChainSupportDetails()
		{
			m_swapChainSupportDetails = FindSwapChainSupport(m_physicalDevice);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		uint32 Device::FindMemoryType(uint32 m_typeFilter, VkMemoryPropertyFlags m_properties)
		{
			VkPhysicalDeviceMemoryProperties memProperties;
			vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

			for (uint32 i = 0; i < memProperties.memoryTypeCount; i++) {
				if ((m_typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & m_properties) == m_properties) {
					return i;
				}
			}
			throw std::runtime_error("failed to find suitable memory type!");
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Device::SelectPhysicalDevice()
		{
			uint32 deviceCount = 0;
			VkInstance const instance = m_renderer.GetInstance();
			vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr); // TODO - rank better?

			if (deviceCount == 0) {
				throw std::runtime_error("failed to find GPUs with Vulkan support!");
			}

			std::vector<VkPhysicalDevice> devices(deviceCount);
			vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

			for (const auto& device : devices) {
				if (IsPhysicalDeviceSuitable(device)) {
					m_physicalDevice = device;
					break;
				}
			}

			if (m_physicalDevice == VK_NULL_HANDLE) {
				throw std::runtime_error("failed to find a suitable GPU!");
			}

			m_deviceQueueFamilies = FindQueueFamilies(m_physicalDevice);
			RecalculateSwapChainSupportDetails();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		bool Device::HasExtensionSupport(VkPhysicalDevice _device) const
		{
			uint32 extensionCount;
			vkEnumerateDeviceExtensionProperties(_device, nullptr, &extensionCount, nullptr);

			std::vector<VkExtensionProperties> availableExtensions(extensionCount);
			vkEnumerateDeviceExtensionProperties(_device, nullptr, &extensionCount, availableExtensions.data());

			std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());

			for (const auto& extension : availableExtensions) {
				requiredExtensions.erase(extension.extensionName);
			}

			return requiredExtensions.empty();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		bool Device::HasSwapChainSupport(VkPhysicalDevice _device) const
		{
			SwapChainSupportDetails swapChainSupport = FindSwapChainSupport(_device);
			return !swapChainSupport.m_formats.empty() && !swapChainSupport.m_presentModes.empty();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		bool Device::IsPhysicalDeviceSuitable(VkPhysicalDevice _device) const
		{
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(_device, &deviceProperties);

			if (deviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				return false;
			}

			VkPhysicalDeviceFeatures deviceFeatures;
			vkGetPhysicalDeviceFeatures(_device, &deviceFeatures);

			if (!deviceFeatures.geometryShader)
			{
				return false;
			}

			if (!HasExtensionSupport(_device))
			{
				return false;
			}

			if (!HasSwapChainSupport(_device))
			{
				return false;
			}

			QueueFamilies const queueFamilies = FindQueueFamilies(_device);
			return queueFamilies.IsValid();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		QueueFamilies Device::FindQueueFamilies(VkPhysicalDevice _device) const
		{
			uint32 queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(_device, &queueFamilyCount, nullptr);

			std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(_device, &queueFamilyCount, queueFamilies.data());

			QueueFamilies queue;
			uint32 i = 0;
			for (const auto& queueFamily : queueFamilies) {
				if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
					queue.m_graphicsFamily = i;
				}

				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(_device, i, m_renderer.GetSurface(), &presentSupport);
				if (presentSupport) {
					queue.m_presentFamily = i;
				}

				if (queue.IsValid())
				{
					break;
				}

				i++;
			}
			return queue;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		SwapChainSupportDetails Device::FindSwapChainSupport(VkPhysicalDevice _device) const
		{
			SwapChainSupportDetails details;
			VkSurfaceKHR const surface = m_renderer.GetSurface();
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_device, surface, &details.m_capabilities);

			uint32 formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(_device, surface, &formatCount, nullptr);

			if (formatCount != 0) {
				details.m_formats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(_device, surface, &formatCount, details.m_formats.data());
			}

			uint32 presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(_device, surface, &presentModeCount, nullptr);

			if (presentModeCount != 0) {
				details.m_presentModes.resize(presentModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(_device, surface, &presentModeCount, details.m_presentModes.data());
			}

			return details;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkDeviceQueueCreateInfo Device::GetDeviceQueueCreateInfo(uint32 queueFamily) const
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;

			static float constexpr queuePriority = 1.0f;
			queueCreateInfo.pQueuePriorities = &queuePriority;

			return queueCreateInfo;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Device::CreateLogicalDevice()
		{
			std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
			std::set<uint32> queueFamilyIndicies{ m_deviceQueueFamilies.m_graphicsFamily.value(), m_deviceQueueFamilies.m_presentFamily.value() };
			for (uint32 index : queueFamilyIndicies)
			{
				queueCreateInfos.push_back(GetDeviceQueueCreateInfo(index));
			}

			VkPhysicalDeviceFeatures deviceFeatures{};

			VkDeviceCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			createInfo.pQueueCreateInfos = queueCreateInfos.data();
			createInfo.queueCreateInfoCount = static_cast<uint32>(queueCreateInfos.size());
			createInfo.pEnabledFeatures = &deviceFeatures;

			createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
			createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

			if (m_renderer.GetValidation().UseValidationLayers()) {
				auto const& validationLayers = m_renderer.GetValidation().GetValidationLayers();
				createInfo.enabledLayerCount = static_cast<uint32>(validationLayers.size());
				createInfo.ppEnabledLayerNames = validationLayers.data();
			}
			else {
				createInfo.enabledLayerCount = 0;
			}

			if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_logicalDevice) != VK_SUCCESS) {
				throw std::runtime_error("failed to create logical device!");
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Device::SetDeviceQueues()
		{
			vkGetDeviceQueue(m_logicalDevice, m_deviceQueueFamilies.m_graphicsFamily.value(), 0, &m_graphicsQueue);
			vkGetDeviceQueue(m_logicalDevice, m_deviceQueueFamilies.m_presentFamily.value(), 0, &m_presentQueue);
		}

	}
}