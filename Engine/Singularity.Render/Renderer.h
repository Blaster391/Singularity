#pragma once
#include <optional>
#include <vulkan/vulkan.hpp>

#include <Singularity.Core/CoreDeclare.h>
#include <Singularity.Render/Device.h>
#include <Singularity.Render/Validation.h>

namespace Singularity
{
	namespace Window
	{
		class Window;
	}

	namespace Render
	{
		class Renderer
		{
		public:
			Renderer(Window::Window& _window);
			~Renderer();

			void Update(float _timeStep);

			VkInstance GetInstance() const { return m_instance; }
			VkSurfaceKHR GetSurface() const { return m_surface; }

			Device const& GetDevice() const { return m_device; }
			Validation const& GetValidation() const { return m_validation; }

		private:
			void Initialize();
			void Shutdown();

			void CreateInstance();
			void CheckExtensions();
			std::vector<const char*> GetRequiredExtensions() const;

			void CreateSurface();

			void CreateSwapChain(); // TODO - SWAPCHAIN.h
			VkSurfaceFormatKHR SelectSwapSurfaceFormat(SwapChainSupportDetails const& _swapChainSupport) const;
			VkPresentModeKHR SelectSwapPresentMode(SwapChainSupportDetails const& _swapChainSupport) const;
			VkExtent2D SelectSwapExtent(SwapChainSupportDetails const& _swapChainSupport) const;

			void CreateImageViews();

			void CreateGraphicsPipeline(); // TODO - PIPELINE.h
			VkShaderModule CreateShaderModule(std::string _filePath); // TODO - SHADER.h

			void CreateRenderPass();

			void CreateFramebuffers();

			void CreateCommandPool();
			void CreateCommandBuffers();

			void CreateSemaphores();

			VkInstance m_instance;
			VkSurfaceKHR m_surface;

			VkSwapchainKHR m_swapChain;
			std::vector<VkImage> m_swapChainImages;
			VkFormat m_swapChainImageFormat;
			VkExtent2D m_swapChainExtent;
			std::vector<VkImageView> m_swapChainImageViews;
			std::vector<VkFramebuffer> m_swapChainFramebuffers;

			VkRenderPass m_renderPass;
			VkPipeline m_graphicsPipeline;
			VkPipelineLayout m_pipelineLayout;

			VkCommandPool m_commandPool;
			std::vector<VkCommandBuffer> m_commandBuffers;

			VkSemaphore m_imageAvailableSemaphore;
			VkSemaphore m_renderFinishedSemaphore;

			Device m_device;
			Validation m_validation;

			Window::Window& m_window;

		};

	}
}


