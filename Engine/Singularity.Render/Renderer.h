#pragma once
#include <optional>
#include <vulkan/vulkan.hpp>

#include <Singularity.Core/CoreDeclare.h>


namespace Singularity
{
	namespace Window
	{
		class Window;
	}

	namespace Render
	{
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

		class Renderer
		{
		public:
			Renderer(Window::Window& _window);
			~Renderer();

			void Update(float _timeStep);

		private:
			

			void Initialize();
			void Shutdown();

			void CreateInstance();
			void CheckExtensions();
			std::vector<const char*> GetRequiredExtensions() const;

			bool UseValidationLayers() const; // TODO - VALIDATION.h
			bool ValidationLayersSupported() const;
			VkDebugUtilsMessengerCreateInfoEXT GetDebugUtilsCreateInfo() const;
			void SetupValidationLayers();

			void SelectPhysicalDevice(); // TODO - DEVICE.h
			bool IsPhysicalDeviceSuitable(VkPhysicalDevice _device) const;
			bool HasExtensionSupport(VkPhysicalDevice _device) const;
			bool HasSwapChainSupport(VkPhysicalDevice _device) const;
			QueueFamilies FindQueueFamilies(VkPhysicalDevice _device) const;
			SwapChainSupportDetails FindSwapChainSupport(VkPhysicalDevice _device) const;

			VkDeviceQueueCreateInfo GetDeviceQueueCreateInfo(uint32 queueFamily) const;
			void CreateLogicalDevice();

			void SetDeviceQueues();

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
			VkDebugUtilsMessengerEXT m_debugMessenger;

			VkDevice m_logicalDevice;
			VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

			VkQueue m_graphicsQueue;
			VkQueue m_presentQueue;
			QueueFamilies m_deviceQueueFamilies;

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

			Window::Window& m_window;

			std::vector<const char*> const m_validationLayers = {
				"VK_LAYER_KHRONOS_validation"
			};

			std::vector<const char*> const m_deviceExtensions = {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME
			};

		};

	}
}


