#include "SwapChain.h"

// Engine
#include <Singularity.Render/Renderer.h>
#include <Singularity.Window/Window.h>

namespace Singularity
{
	namespace Render
	{
		//////////////////////////////////////////////////////////////////////////////////////
		SwapChain::SwapChain(Renderer const& _renderer)
			:m_renderer(_renderer)
		{
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void SwapChain::Initialize()
		{
			CreateSwapChain();
			CreateImageViews();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void SwapChain::Shutdown()
		{
			for (auto imageView : m_swapChainImageViews) {
				vkDestroyImageView(m_renderer.GetDevice().GetLogicalDevice(), imageView, nullptr);
			}

			vkDestroySwapchainKHR(m_renderer.GetDevice().GetLogicalDevice(), m_swapChain, nullptr);

		}

		//////////////////////////////////////////////////////////////////////////////////////
		void SwapChain::CreateSwapChain()
		{
			SwapChainSupportDetails const swapChainSupport = m_renderer.GetDevice().GetSwapChainSupportDetails();
			m_swapChainExtent = SelectSwapExtent(swapChainSupport);

			VkSurfaceFormatKHR surfaceFormat = SelectSwapSurfaceFormat(swapChainSupport);
			VkPresentModeKHR presentMode = SelectSwapPresentMode(swapChainSupport);

			uint32 imageCount = swapChainSupport.m_capabilities.minImageCount + 1;
			if ((swapChainSupport.m_capabilities.maxImageCount > 0) && (imageCount > swapChainSupport.m_capabilities.maxImageCount)) {
				imageCount = swapChainSupport.m_capabilities.maxImageCount;
			}

			VkSwapchainCreateInfoKHR createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			createInfo.surface = m_renderer.GetSurface();
			createInfo.minImageCount = imageCount;
			createInfo.imageFormat = surfaceFormat.format;
			createInfo.imageColorSpace = surfaceFormat.colorSpace;
			createInfo.imageExtent = m_swapChainExtent;
			createInfo.imageArrayLayers = 1;
			createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;


			QueueFamilies const& queueFamilies = m_renderer.GetDevice().GetQueueFamilies();
			uint32 queueFamilyIndices[] = { queueFamilies.m_graphicsFamily.value(), queueFamilies.m_presentFamily.value() };

			if (queueFamilies.m_graphicsFamily != queueFamilies.m_presentFamily) {
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

			VkDevice const logicalDevice = m_renderer.GetDevice().GetLogicalDevice();
			if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
				throw std::runtime_error("failed to create swap chain!");
			}

			vkGetSwapchainImagesKHR(logicalDevice, m_swapChain, &imageCount, nullptr);
			m_swapChainImages.resize(imageCount);
			vkGetSwapchainImagesKHR(logicalDevice, m_swapChain, &imageCount, m_swapChainImages.data());

			m_swapChainImageFormat = surfaceFormat.format;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkSurfaceFormatKHR SwapChain::SelectSwapSurfaceFormat(SwapChainSupportDetails const& _swapChainSupport) const
		{
			for (const auto& availableFormat : _swapChainSupport.m_formats) {
				if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
					return availableFormat;
				}
			}

			return _swapChainSupport.m_formats[0];
		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkPresentModeKHR SwapChain::SelectSwapPresentMode(SwapChainSupportDetails const& _swapChainSupport) const
		{
			for (const auto& availablePresentMode : _swapChainSupport.m_presentModes) {
				if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
					return availablePresentMode;
				}
			}

			return VK_PRESENT_MODE_FIFO_KHR;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkExtent2D SwapChain::SelectSwapExtent(SwapChainSupportDetails const& _swapChainSupport) const
		{
			if (_swapChainSupport.m_capabilities.currentExtent.width != UINT32_MAX) {
				return _swapChainSupport.m_capabilities.currentExtent;
			}
			else {
				VkExtent2D actualExtent = { m_renderer.GetWindow().GetWidth(), m_renderer.GetWindow().GetHeight() };
				actualExtent.width = std::max(_swapChainSupport.m_capabilities.minImageExtent.width, std::min(_swapChainSupport.m_capabilities.maxImageExtent.width, actualExtent.width));
				actualExtent.height = std::max(_swapChainSupport.m_capabilities.minImageExtent.height, std::min(_swapChainSupport.m_capabilities.maxImageExtent.height, actualExtent.height));
				return actualExtent;
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void SwapChain::CreateImageViews()
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

				if (vkCreateImageView(m_renderer.GetDevice().GetLogicalDevice(), &createInfo, nullptr, &m_swapChainImageViews[i]) != VK_SUCCESS) {
					throw std::runtime_error("failed to create image views!");
				}
			}
		}
	}
}