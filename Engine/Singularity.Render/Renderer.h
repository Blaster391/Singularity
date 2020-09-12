#pragma once
#include <optional>
#include <vulkan/vulkan.hpp>

#include <Singularity.Core/CoreDeclare.h>
#include <Singularity.Render/Device.h>
#include <Singularity.Render/GenericUniformBufferObject.h>
#include <Singularity.Render/SwapChain.h>
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
			Window::Window const& GetWindow() const { return m_window; }

			void RebuildSwapChain();

		private:
			void Initialize();
			void Shutdown();

			void UpdateUniformBuffers(uint32 _imageIndex);

			void CreateDescriptorSetLayout();
			void CreatePipeline();// Can't think of better name (Framebuffers + Pipeline)
			void DestroyPipeline();

			void CreateInstance();
			void CheckExtensions();
			std::vector<const char*> GetRequiredExtensions() const;

			void CreateSurface();

			void CreateGraphicsPipeline(); // TODO - PIPELINE.h
			VkShaderModule CreateShaderModule(std::string _filePath); // TODO - SHADER.h

			void CreateRenderPass();

			void CreateFramebuffers();

			void CreateVertexBuffer();
			void CreateUniformBuffers();
			void CreateDescriptorPool();
			void CreateDescriptorSets();

			void CreateCommandPool();
			void CreateCommandBuffers();

			void CreateSemaphores();

			void CreateBuffer(VkBufferCreateInfo _createInfo, VkMemoryPropertyFlags _properties, VkBuffer& o_buffer, VkDeviceMemory& o_bufferMemory); // TODO Hide buffers in their own class????

			VkInstance m_instance;
			VkSurfaceKHR m_surface;

			std::vector<VkFramebuffer> m_swapChainFramebuffers;

			VkDescriptorSetLayout m_descriptorSetLayout;
			VkDescriptorPool m_descriptorPool;
			std::vector<VkDescriptorSet> m_descriptorSets;

			VkRenderPass m_renderPass;
			VkPipeline m_graphicsPipeline;
			VkPipelineLayout m_pipelineLayout;

			VkBuffer m_vertexBuffer; // TODO move all these into their own classes, don't rebuild on window resize
			VkDeviceMemory m_vertexBufferMemory; // TODO index buffers

			std::vector<VkBuffer> m_uniformBuffers;
			std::vector<VkDeviceMemory> m_uniformBuffersMemory;

			VkCommandPool m_commandPool;
			std::vector<VkCommandBuffer> m_commandBuffers;

			VkSemaphore m_imageAvailableSemaphore;
			VkSemaphore m_renderFinishedSemaphore;

			Device m_device;
			Validation m_validation;
			SwapChain m_swapChain;

			Window::Window& m_window;

		};

	}
}


