#pragma once
#include <vulkan/vulkan.hpp>

namespace Singularity
{
	namespace Render
	{
		class Renderer;
		struct SwapChainSupportDetails;

		class SwapChain
		{
		public:
			SwapChain(Renderer const& _renderer);
			~SwapChain() {}

			void Initialize();
			void Shutdown();

			VkSwapchainKHR GetSwapChain() const { return m_swapChain; }
			VkExtent2D GetExtent() const { return m_swapChainExtent; }
			VkFormat GetFormat() const { return m_swapChainImageFormat; }
			std::vector<VkImageView> const& GetImageViews() const { return m_swapChainImageViews; }

		private:
			void CreateSwapChain();
			VkSurfaceFormatKHR SelectSwapSurfaceFormat(SwapChainSupportDetails const& _swapChainSupport) const;
			VkPresentModeKHR SelectSwapPresentMode(SwapChainSupportDetails const& _swapChainSupport) const;
			VkExtent2D SelectSwapExtent(SwapChainSupportDetails const& _swapChainSupport) const;
			void CreateImageViews(); // TODO link with CreateImageView

			Renderer const& m_renderer;

			VkSwapchainKHR m_swapChain;
			std::vector<VkImage> m_swapChainImages;
			VkFormat m_swapChainImageFormat;
			VkExtent2D m_swapChainExtent;
			std::vector<VkImageView> m_swapChainImageViews;

		};


	}
}