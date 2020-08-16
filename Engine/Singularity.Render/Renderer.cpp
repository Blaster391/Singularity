#include "Renderer.h"

// External Includes
#include <iostream>
#include <set>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_win32.h>

// Engine Includes
#include <Singularity.IO/IO.h>
#include <Singularity.Window/Window.h>

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

		//////////////////////////////////////////////////////////////////////////////////////
		Renderer::Renderer(Window::Window& _window)
			: m_window(_window)
		{
			Initialize();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		Renderer::~Renderer()
		{
			Shutdown();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::Update(float _timeStep)
		{
			uint32_t imageIndex;
			vkAcquireNextImageKHR(m_logicalDevice, m_swapChain, UINT64_MAX, m_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

			VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphore };
			VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];

			VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphore };
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;

			if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
				throw std::runtime_error("failed to submit draw command buffer!");
			}

			VkPresentInfoKHR presentInfo{};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = signalSemaphores;

			VkSwapchainKHR swapChains[] = { m_swapChain };
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = swapChains;
			presentInfo.pImageIndices = &imageIndex;
			presentInfo.pResults = nullptr;

			vkQueuePresentKHR(m_presentQueue, &presentInfo);

			vkQueueWaitIdle(m_presentQueue); // TODO not optimal
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::Initialize()
		{
			CreateInstance();
			CheckExtensions();
			SetupValidationLayers();
			CreateSurface();
			SelectPhysicalDevice();
			CreateLogicalDevice();
			SetDeviceQueues();
			CreateSwapChain();
			CreateImageViews();
			CreateRenderPass();
			CreateGraphicsPipeline();
			CreateFramebuffers();
			CreateCommandPool();
			CreateCommandBuffers();
			CreateSemaphores();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::Shutdown()
		{
			vkDeviceWaitIdle(m_logicalDevice);

			vkDestroySemaphore(m_logicalDevice, m_renderFinishedSemaphore, nullptr);
			vkDestroySemaphore(m_logicalDevice, m_imageAvailableSemaphore, nullptr);

			vkDestroyCommandPool(m_logicalDevice, m_commandPool, nullptr);

			for (auto framebuffer : m_swapChainFramebuffers) {
				vkDestroyFramebuffer(m_logicalDevice, framebuffer, nullptr);
			}

			vkDestroyPipeline(m_logicalDevice, m_graphicsPipeline, nullptr);
			vkDestroyPipelineLayout(m_logicalDevice, m_pipelineLayout, nullptr);
			vkDestroyRenderPass(m_logicalDevice, m_renderPass, nullptr);

			for (auto imageView : m_swapChainImageViews) {
				vkDestroyImageView(m_logicalDevice, imageView, nullptr);
			}

			vkDestroySwapchainKHR(m_logicalDevice, m_swapChain, nullptr);

			vkDestroyDevice(m_logicalDevice, nullptr);

			if (UseValidationLayers()) {
				DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
			}

			vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
			vkDestroyInstance(m_instance, nullptr);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		bool Renderer::UseValidationLayers() const
		{
#if NDEBUG
			return false;
#else
			return true;
#endif
		}

		//////////////////////////////////////////////////////////////////////////////////////
		bool Renderer::ValidationLayersSupported() const
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
		VkDebugUtilsMessengerCreateInfoEXT Renderer::GetDebugUtilsCreateInfo() const
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
		void Renderer::SetupValidationLayers()
		{
			if (!UseValidationLayers())
			{
				return;
			}

			VkDebugUtilsMessengerCreateInfoEXT createInfo = GetDebugUtilsCreateInfo();

			if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
				throw std::runtime_error("failed to set up debug messenger!");
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::SelectPhysicalDevice()
		{
			uint32 deviceCount = 0;
			vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr); // TODO - rank better?

			if (deviceCount == 0) {
				throw std::runtime_error("failed to find GPUs with Vulkan support!");
			}

			std::vector<VkPhysicalDevice> devices(deviceCount);
			vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

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
		}

		//////////////////////////////////////////////////////////////////////////////////////
		bool Renderer::HasExtensionSupport(VkPhysicalDevice _device) const
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
		bool Renderer::HasSwapChainSupport(VkPhysicalDevice _device) const
		{
			SwapChainSupportDetails swapChainSupport = FindSwapChainSupport(_device);
			return !swapChainSupport.m_formats.empty() && !swapChainSupport.m_presentModes.empty();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		bool Renderer::IsPhysicalDeviceSuitable(VkPhysicalDevice _device) const
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
		QueueFamilies Renderer::FindQueueFamilies(VkPhysicalDevice _device) const
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
				vkGetPhysicalDeviceSurfaceSupportKHR(_device, i, m_surface, &presentSupport);
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
		SwapChainSupportDetails Renderer::FindSwapChainSupport(VkPhysicalDevice _device) const
		{
			SwapChainSupportDetails details;
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_device, m_surface, &details.m_capabilities);

			uint32 formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(_device, m_surface, &formatCount, nullptr);

			if (formatCount != 0) {
				details.m_formats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(_device, m_surface, &formatCount, details.m_formats.data());
			}

			uint32 presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(_device, m_surface, &presentModeCount, nullptr);

			if (presentModeCount != 0) {
				details.m_presentModes.resize(presentModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(_device, m_surface, &presentModeCount, details.m_presentModes.data());
			}

			return details;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkDeviceQueueCreateInfo Renderer::GetDeviceQueueCreateInfo(uint32 queueFamily) const
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
		void Renderer::CreateLogicalDevice()
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

			if (UseValidationLayers()) {
				createInfo.enabledLayerCount = static_cast<uint32>(m_validationLayers.size());
				createInfo.ppEnabledLayerNames = m_validationLayers.data();
			}
			else {
				createInfo.enabledLayerCount = 0;
			}

			if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_logicalDevice) != VK_SUCCESS) {
				throw std::runtime_error("failed to create logical device!");
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::SetDeviceQueues()
		{
			vkGetDeviceQueue(m_logicalDevice, m_deviceQueueFamilies.m_graphicsFamily.value(), 0, &m_graphicsQueue);
			vkGetDeviceQueue(m_logicalDevice, m_deviceQueueFamilies.m_presentFamily.value(), 0, &m_presentQueue);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateSurface()
		{
			VkWin32SurfaceCreateInfoKHR createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			createInfo.hwnd = m_window.GetHandle();
			createInfo.hinstance = GetModuleHandle(nullptr);

			if (vkCreateWin32SurfaceKHR(m_instance, &createInfo, nullptr, &m_surface) != VK_SUCCESS) {
				throw std::runtime_error("failed to create window surface!");
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateSwapChain()
		{
			SwapChainSupportDetails const swapChainSupport = FindSwapChainSupport(m_physicalDevice);
			m_swapChainExtent = SelectSwapExtent(swapChainSupport);

			VkSurfaceFormatKHR surfaceFormat = SelectSwapSurfaceFormat(swapChainSupport);
			VkPresentModeKHR presentMode = SelectSwapPresentMode(swapChainSupport);

			uint32 imageCount = swapChainSupport.m_capabilities.minImageCount + 1;
			if ((swapChainSupport.m_capabilities.maxImageCount > 0) && (imageCount > swapChainSupport.m_capabilities.maxImageCount)) {
				imageCount = swapChainSupport.m_capabilities.maxImageCount;
			}

			VkSwapchainCreateInfoKHR createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			createInfo.surface = m_surface;
			createInfo.minImageCount = imageCount;
			createInfo.imageFormat = surfaceFormat.format;
			createInfo.imageColorSpace = surfaceFormat.colorSpace;
			createInfo.imageExtent = m_swapChainExtent;
			createInfo.imageArrayLayers = 1;
			createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			uint32 queueFamilyIndices[] = { m_deviceQueueFamilies.m_graphicsFamily.value(), m_deviceQueueFamilies.m_presentFamily.value() };

			if (m_deviceQueueFamilies.m_graphicsFamily != m_deviceQueueFamilies.m_presentFamily) {
				createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
				createInfo.queueFamilyIndexCount = 2;
				createInfo.pQueueFamilyIndices = queueFamilyIndices;
			}
			else {
				createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
				createInfo.queueFamilyIndexCount = 0; // Optional
				createInfo.pQueueFamilyIndices = nullptr; // Optional
			}

			createInfo.preTransform = swapChainSupport.m_capabilities.currentTransform;
			createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

			createInfo.presentMode = presentMode;
			createInfo.clipped = VK_TRUE;

			createInfo.oldSwapchain = VK_NULL_HANDLE;

			if (vkCreateSwapchainKHR(m_logicalDevice, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
				throw std::runtime_error("failed to create swap chain!");
			}

			vkGetSwapchainImagesKHR(m_logicalDevice, m_swapChain, &imageCount, nullptr);
			m_swapChainImages.resize(imageCount);
			vkGetSwapchainImagesKHR(m_logicalDevice, m_swapChain, &imageCount, m_swapChainImages.data());

			m_swapChainImageFormat = surfaceFormat.format;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkSurfaceFormatKHR Renderer::SelectSwapSurfaceFormat(SwapChainSupportDetails const& _swapChainSupport) const
		{
			for (const auto& availableFormat : _swapChainSupport.m_formats) {
				if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
					return availableFormat;
				}
			}

			return _swapChainSupport.m_formats[0];
		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkPresentModeKHR Renderer::SelectSwapPresentMode(SwapChainSupportDetails const& _swapChainSupport) const
		{
			for (const auto& availablePresentMode : _swapChainSupport.m_presentModes) {
				if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
					return availablePresentMode;
				}
			}

			return VK_PRESENT_MODE_FIFO_KHR;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkExtent2D Renderer::SelectSwapExtent(SwapChainSupportDetails const& _swapChainSupport) const
		{
			if (_swapChainSupport.m_capabilities.currentExtent.width != UINT32_MAX) {
				return _swapChainSupport.m_capabilities.currentExtent;
			}
			else {
				VkExtent2D actualExtent = { m_window.GetWidth(), m_window.GetHeight() };
				actualExtent.width = std::max(_swapChainSupport.m_capabilities.minImageExtent.width, std::min(_swapChainSupport.m_capabilities.maxImageExtent.width, actualExtent.width));
				actualExtent.height = std::max(_swapChainSupport.m_capabilities.minImageExtent.height, std::min(_swapChainSupport.m_capabilities.maxImageExtent.height, actualExtent.height));
				return actualExtent;
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateImageViews()
		{
			m_swapChainImageViews.resize(m_swapChainImages.size());
			for (size_t i = 0; i < m_swapChainImages.size(); i++) {
				VkImageViewCreateInfo createInfo{};
				createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				createInfo.image = m_swapChainImages[i];

				createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				createInfo.format = m_swapChainImageFormat;

				createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				createInfo.subresourceRange.baseMipLevel = 0;
				createInfo.subresourceRange.levelCount = 1;
				createInfo.subresourceRange.baseArrayLayer = 0;
				createInfo.subresourceRange.layerCount = 1;

				if (vkCreateImageView(m_logicalDevice, &createInfo, nullptr, &m_swapChainImageViews[i]) != VK_SUCCESS) {
					throw std::runtime_error("failed to create image views!");
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateGraphicsPipeline()
		{
			VkShaderModule vertexShaderModule = CreateShaderModule(std::string(DATA_DIRECTORY) + "Shaders/Vertex/shader_vert.spv"); // TODO eewwwww
			VkShaderModule fragmentShaderModule = CreateShaderModule(std::string(DATA_DIRECTORY) + "Shaders/Fragment/shader_frag.spv");

			VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
			vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vertShaderStageInfo.module = vertexShaderModule;
			vertShaderStageInfo.pName = "main";

			VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
			fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragShaderStageInfo.module = fragmentShaderModule;
			fragShaderStageInfo.pName = "main";

			VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

			VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
			vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputInfo.vertexBindingDescriptionCount = 0;
			vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
			vertexInputInfo.vertexAttributeDescriptionCount = 0;
			vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

			VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
			inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			inputAssembly.primitiveRestartEnable = VK_FALSE;

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)m_swapChainExtent.width;
			viewport.height = (float)m_swapChainExtent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = m_swapChainExtent;

			VkPipelineViewportStateCreateInfo viewportState{};
			viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportState.viewportCount = 1;
			viewportState.pViewports = &viewport;
			viewportState.scissorCount = 1;
			viewportState.pScissors = &scissor;

			VkPipelineRasterizationStateCreateInfo rasterizer{};
			rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizer.depthClampEnable = VK_FALSE;
			rasterizer.rasterizerDiscardEnable = VK_FALSE;
			rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizer.lineWidth = 1.0f;
			rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
			rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
			rasterizer.depthBiasEnable = VK_FALSE;
			rasterizer.depthBiasConstantFactor = 0.0f;
			rasterizer.depthBiasClamp = 0.0f;
			rasterizer.depthBiasSlopeFactor = 0.0f;

			VkPipelineMultisampleStateCreateInfo multisampling{};
			multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampling.sampleShadingEnable = VK_FALSE;
			multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
			multisampling.minSampleShading = 1.0f;
			multisampling.pSampleMask = nullptr;
			multisampling.alphaToCoverageEnable = VK_FALSE;
			multisampling.alphaToOneEnable = VK_FALSE;

			VkPipelineColorBlendAttachmentState colorBlendAttachment{};
			colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachment.blendEnable = VK_TRUE;
			colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

			VkPipelineColorBlendStateCreateInfo colorBlending{};
			colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlending.logicOpEnable = VK_FALSE;
			colorBlending.logicOp = VK_LOGIC_OP_COPY;
			colorBlending.attachmentCount = 1;
			colorBlending.pAttachments = &colorBlendAttachment;
			colorBlending.blendConstants[0] = 0.0f;
			colorBlending.blendConstants[1] = 0.0f;
			colorBlending.blendConstants[2] = 0.0f;
			colorBlending.blendConstants[3] = 0.0f;

			VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = 0;
			pipelineLayoutInfo.pSetLayouts = nullptr;
			pipelineLayoutInfo.pushConstantRangeCount = 0;
			pipelineLayoutInfo.pPushConstantRanges = nullptr;

			if (vkCreatePipelineLayout(m_logicalDevice, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
				throw std::runtime_error("failed to create pipeline layout!");
			}

			VkGraphicsPipelineCreateInfo pipelineInfo{};
			pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineInfo.stageCount = 2;
			pipelineInfo.pStages = shaderStages;
			pipelineInfo.pVertexInputState = &vertexInputInfo;
			pipelineInfo.pInputAssemblyState = &inputAssembly;
			pipelineInfo.pViewportState = &viewportState;
			pipelineInfo.pRasterizationState = &rasterizer;
			pipelineInfo.pMultisampleState = &multisampling;
			pipelineInfo.pDepthStencilState = nullptr;
			pipelineInfo.pColorBlendState = &colorBlending;
			pipelineInfo.pDynamicState = nullptr;
			pipelineInfo.layout = m_pipelineLayout;
			pipelineInfo.renderPass = m_renderPass;
			pipelineInfo.subpass = 0;
			pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
			pipelineInfo.basePipelineIndex = -1;

			if (vkCreateGraphicsPipelines(m_logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
				throw std::runtime_error("failed to create graphics pipeline!");
			}

			vkDestroyShaderModule(m_logicalDevice, fragmentShaderModule, nullptr);
			vkDestroyShaderModule(m_logicalDevice, vertexShaderModule, nullptr);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkShaderModule Renderer::CreateShaderModule(std::string _filePath)
		{
			auto shaderCode = IO::ReadFile(_filePath);

			VkShaderModuleCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = shaderCode.size();
			createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

			VkShaderModule shaderModule;
			if (vkCreateShaderModule(m_logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
				throw std::runtime_error("failed to create shader module!");
			}

			return shaderModule;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateRenderPass()
		{
			VkAttachmentDescription colorAttachment{};
			colorAttachment.format = m_swapChainImageFormat;
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference colorAttachmentRef{};
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;

			VkRenderPassCreateInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.pAttachments = &colorAttachment;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;


			VkSubpassDependency dependency{};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.srcAccessMask = 0;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			renderPassInfo.dependencyCount = 1;
			renderPassInfo.pDependencies = &dependency;


			if (vkCreateRenderPass(m_logicalDevice, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
				throw std::runtime_error("failed to create render pass!");
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateFramebuffers()
		{
			m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

			for (size_t i = 0; i < m_swapChainImageViews.size(); i++) {
				VkImageView attachments[] = {
					m_swapChainImageViews[i]
				};

				VkFramebufferCreateInfo framebufferInfo{};
				framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferInfo.renderPass = m_renderPass;
				framebufferInfo.attachmentCount = 1;
				framebufferInfo.pAttachments = attachments;
				framebufferInfo.width = m_swapChainExtent.width;
				framebufferInfo.height = m_swapChainExtent.height;
				framebufferInfo.layers = 1;

				if (vkCreateFramebuffer(m_logicalDevice, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS) {
					throw std::runtime_error("failed to create framebuffer!");
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateCommandPool()
		{
			VkCommandPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.queueFamilyIndex = m_deviceQueueFamilies.m_graphicsFamily.value();
			poolInfo.flags = 0;

			if (vkCreateCommandPool(m_logicalDevice, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
				throw std::runtime_error("failed to create command pool!");
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateCommandBuffers()
		{
			m_commandBuffers.resize(m_swapChainFramebuffers.size());

			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = m_commandPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = (uint32)m_commandBuffers.size();

			if (vkAllocateCommandBuffers(m_logicalDevice, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
				throw std::runtime_error("failed to allocate command buffers!");
			}

			for (size_t i = 0; i < m_commandBuffers.size(); i++) {
				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = 0; // Optional
				beginInfo.pInheritanceInfo = nullptr; // Optional

				if (vkBeginCommandBuffer(m_commandBuffers[i], &beginInfo) != VK_SUCCESS) {
					throw std::runtime_error("failed to begin recording command buffer!");
				}

				VkRenderPassBeginInfo renderPassInfo{};
				renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassInfo.renderPass = m_renderPass;
				renderPassInfo.framebuffer = m_swapChainFramebuffers[i];
				renderPassInfo.renderArea.offset = { 0, 0 };
				renderPassInfo.renderArea.extent = m_swapChainExtent;

				VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
				renderPassInfo.clearValueCount = 1;
				renderPassInfo.pClearValues = &clearColor;

				vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

				vkCmdDraw(m_commandBuffers[i], 3, 1, 0, 0); // Draw a triangle lul

				vkCmdEndRenderPass(m_commandBuffers[i]);

				if (vkEndCommandBuffer(m_commandBuffers[i]) != VK_SUCCESS) {
					throw std::runtime_error("failed to record command buffer!");
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateSemaphores()
		{
			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			if (vkCreateSemaphore(m_logicalDevice, &semaphoreInfo, nullptr, &m_imageAvailableSemaphore) != VK_SUCCESS ||
				vkCreateSemaphore(m_logicalDevice, &semaphoreInfo, nullptr, &m_renderFinishedSemaphore) != VK_SUCCESS) {

				throw std::runtime_error("failed to create semaphores!");
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateInstance()
		{
			if (UseValidationLayers() && !ValidationLayersSupported()) {
				throw std::runtime_error("validation layers requested, but not available!");
			}

			VkApplicationInfo appInfo{};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = "Test";
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.pEngineName = "Singularity";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.apiVersion = VK_API_VERSION_1_0;


			std::vector<const char*> const extensions = GetRequiredExtensions();
			VkInstanceCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			createInfo.pApplicationInfo = &appInfo;
			createInfo.enabledExtensionCount = static_cast<uint32>(extensions.size());
			createInfo.ppEnabledExtensionNames = extensions.data();
			if (UseValidationLayers()) {
				createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
				createInfo.ppEnabledLayerNames = m_validationLayers.data();
				VkDebugUtilsMessengerCreateInfoEXT const debugCreateInfo = GetDebugUtilsCreateInfo();
				createInfo.pNext = &debugCreateInfo;
			}
			else {
				createInfo.enabledLayerCount = 0;
				createInfo.pNext = nullptr;
			}

			if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
				throw std::runtime_error("failed to create instance!");
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CheckExtensions()
		{
			uint32 extensionCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
			std::cout << "available extensions:\n";
			std::vector<VkExtensionProperties> extensions(extensionCount);
			vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

			for (const auto& extension : extensions) {
				std::cout << '\t' << extension.extensionName << '\n';
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		std::vector<const char*> Renderer::GetRequiredExtensions() const
		{
			Window::WindowExtensionsInfo const windowExtensions = m_window.GetExtensions();

			std::vector<const char*> extensions(windowExtensions.m_extensions, windowExtensions.m_extensions + windowExtensions.m_extensionCount);

			if (UseValidationLayers()) {
				extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			}

			return extensions;
		}
	}
}