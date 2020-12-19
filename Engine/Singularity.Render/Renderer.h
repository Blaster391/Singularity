#pragma once
#include <optional>
#include <vulkan/vulkan.hpp>

#include <Singularity.Core/CoreDeclare.h>
#include <Singularity.Render/Device.h>
#include <Singularity.Render/Image.h>
#include <Singularity.Render/GenericUniformBufferObject.h>
#include <Singularity.Render/Mesh.h>
#include <Singularity.Render/RenderObject.h>
#include <Singularity.Render/SwapChain.h>
#include <Singularity.Render/Texture.h>
#include <Singularity.Render/UniformBufferAllocator.h>
#include <Singularity.Render/Validation.h>

namespace Singularity
{
	namespace Window
	{
		class Window;
	}

	namespace Render
	{

		class Buffer;

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
			SwapChain const& GetSwapChain() const { return m_swapChain; }
			UniformBufferAllocator& GetUniformBufferAllocator() { return m_uniformBufferAllocator; }

			Window::Window const& GetWindow() const { return m_window; }

			void RebuildSwapChain();

			VkCommandBuffer BeginSingleTimeCommands(); // TODO extract out command buffer stuff
			void EndSingleTimeCommands(VkCommandBuffer _commandBuffer);

			VkDescriptorPool GetDescriptorPool() const { return m_descriptorPool; }
			VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_descriptorSetLayout; }
			VkPipelineLayout GetPipelineLayout() const { return  m_pipelineLayout; }

		private:
			void Initialize();
			void Shutdown();

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

			void CreateDescriptorPool();

			void CreateCommandPool();
			void CreateCommandBuffers();

			void CreateSyncObjects();

			void CreateTextureImage();

			void CreateDepthResources();
			VkFormat FindDepthFormat() const;
			bool HasStencilComponent(VkFormat _format) const;
			VkFormat FindSupportedFormat(const std::vector<VkFormat>& _candidates, VkImageTiling _tiling, VkFormatFeatureFlags _features) const; // TODO move to device???

			VkInstance m_instance;
			VkSurfaceKHR m_surface;

			std::vector<VkFramebuffer> m_swapChainFramebuffers;

			VkDescriptorSetLayout m_descriptorSetLayout; // TO OWN THING
			VkDescriptorPool m_descriptorPool;

			VkRenderPass m_renderPass;
			VkPipeline m_graphicsPipeline;
			VkPipelineLayout m_pipelineLayout;
			Image m_depthImage;

			VkCommandPool m_commandPool;
			std::vector<VkCommandBuffer> m_commandBuffers;

			std::vector<VkSemaphore> m_imageAvailableSemaphores;
			std::vector<VkSemaphore> m_renderFinishedSemaphores;
			std::vector<VkFence> m_inFlightFences;
			std::vector<VkFence> m_imagesInFlight;

			Device m_device;
			Validation m_validation;
			SwapChain m_swapChain;
			UniformBufferAllocator m_uniformBufferAllocator;

			Window::Window& m_window;

			static uint64 constexpr MAX_FRAMES_IN_FLIGHT = 2u;
			uint64 m_currentFrame = 0u;

			Texture m_texture;
			Mesh m_testMesh;
			Mesh m_testMesh2;

			RenderObject m_testObject;
		};

	}
}


