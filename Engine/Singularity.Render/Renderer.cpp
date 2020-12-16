#include "Renderer.h"

// External Includes
#define GLM_FORCE_RADIANS
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <set>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_win32.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

// Engine Includes
#include <Singularity.IO/IO.h>
#include <Singularity.Render/Mesh.h>
#include <Singularity.Window/Window.h>

namespace Singularity
{
	namespace Render
	{

		//////////////////////////////////////////////////////////////////////////////////////
		Renderer::Renderer(Window::Window& _window)
			: 
			m_device(*this),
			m_validation(*this),
			m_swapChain(*this),
			m_window(_window)
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
			uint32 imageIndex;
			VkResult const result = vkAcquireNextImageKHR(m_device.GetLogicalDevice(), m_swapChain.GetSwapChain(), UINT64_MAX, m_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				RebuildSwapChain();
				return;
			}
			else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
				throw std::runtime_error("failed to acquire swap chain image!");
			}

			UpdateUniformBuffers(imageIndex);

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

			if (vkQueueSubmit(m_device.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
				throw std::runtime_error("failed to submit draw command buffer!");
			}

			VkPresentInfoKHR presentInfo{};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = signalSemaphores;

			VkSwapchainKHR swapChains[] = { m_swapChain.GetSwapChain() };
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = swapChains;
			presentInfo.pImageIndices = &imageIndex;
			presentInfo.pResults = nullptr;

			vkQueuePresentKHR(m_device.GetPresentQueue(), &presentInfo);

			vkQueueWaitIdle(m_device.GetPresentQueue()); // TODO not optimal
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::RebuildSwapChain()
		{
			vkQueueWaitIdle(m_device.GetPresentQueue());

			// Cleanup old swapchain
			DestroyPipeline();
			m_swapChain.Shutdown();

			// Init new swapchain
			m_device.RecalculateSwapChainSupportDetails();
			m_swapChain.Initialize();
			CreatePipeline();

		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::Initialize()
		{
			CreateInstance();
			CheckExtensions();

			m_validation.Initialize();

			CreateSurface();
			
			m_device.Initialize();
			m_swapChain.Initialize();

			CreateDescriptorSetLayout();
			CreatePipeline();

		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::Shutdown()
		{
			DestroyPipeline();

			VkDevice const device = m_device.GetLogicalDevice();
			vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);

			m_swapChain.Shutdown();

			vkDestroySampler(device, m_textureSampler, nullptr);
			vkDestroyImageView(device, m_textureImageView, nullptr);
			vkDestroyImage(device, m_textureImage, nullptr); // TODO - maybe rebuild????
			vkFreeMemory(device, m_textureImageMemory, nullptr);

			m_device.Shutdown();

			m_validation.Shutdown();


			vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
			vkDestroyInstance(m_instance, nullptr);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::UpdateUniformBuffers(uint32 _imageIndex)
		{
			static auto startTime = std::chrono::high_resolution_clock::now();

			auto currentTime = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

			GenericUniformBufferObject ubo{};
			ubo.m_model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

			ubo.m_view = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

			VkExtent2D const swapChainExtent = m_swapChain.GetExtent();
			ubo.m_projection = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);

			ubo.m_projection[1][1] *= -1;

			void* data;
			vkMapMemory(m_device.GetLogicalDevice(), m_uniformBuffersMemory[_imageIndex], 0, sizeof(ubo), 0, &data);
			memcpy(data, &ubo, sizeof(ubo));
			vkUnmapMemory(m_device.GetLogicalDevice(), m_uniformBuffersMemory[_imageIndex]);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateDescriptorSetLayout()
		{
			VkDescriptorSetLayoutBinding uboLayoutBinding{};
			uboLayoutBinding.binding = 0;
			uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			uboLayoutBinding.descriptorCount = 1;
			uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

			VkDescriptorSetLayoutBinding samplerLayoutBinding{};
			samplerLayoutBinding.binding = 1;
			samplerLayoutBinding.descriptorCount = 1;
			samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			samplerLayoutBinding.pImmutableSamplers = nullptr;
			samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

			std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };
			VkDescriptorSetLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = static_cast<uint32>(bindings.size());;
			layoutInfo.pBindings = bindings.data();

			if (vkCreateDescriptorSetLayout(m_device.GetLogicalDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
				throw std::runtime_error("failed to create descriptor set layout!");
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreatePipeline()
		{
			CreateRenderPass();
			CreateGraphicsPipeline();
			
			CreateVertexBuffer();
			CreateUniformBuffers();
			CreateDescriptorPool();

			CreateCommandPool(); // TODO ordering and cleanup and not rebuilding stuff i shouldn't

			CreateDepthResources();
			CreateFramebuffers();

			CreateTextureImage();
			CreateTextureImageView();
			CreateTextureSampler();

			CreateDescriptorSets();
			
			CreateCommandBuffers();
			CreateSemaphores();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::DestroyPipeline()
		{
			VkDevice const logicalDevice = m_device.GetLogicalDevice();
			vkDestroySemaphore(logicalDevice, m_renderFinishedSemaphore, nullptr);
			vkDestroySemaphore(logicalDevice, m_imageAvailableSemaphore, nullptr);

			vkDestroyImageView(logicalDevice, m_depthImageView, nullptr);
			vkDestroyImage(logicalDevice, m_depthImage, nullptr);
			vkFreeMemory(logicalDevice, m_depthImageMemory, nullptr);


			vkDestroyCommandPool(logicalDevice, m_commandPool, nullptr);

			vkDestroyBuffer(logicalDevice, m_vertexBuffer, nullptr);
			vkFreeMemory(logicalDevice, m_vertexBufferMemory, nullptr);

			vkDestroyDescriptorPool(logicalDevice, m_descriptorPool, nullptr);

			for (size_t i = 0; i < m_swapChain.GetImageViews().size(); i++) {
				vkDestroyBuffer(logicalDevice, m_uniformBuffers[i], nullptr);
				vkFreeMemory(logicalDevice, m_uniformBuffersMemory[i], nullptr);
			}

			for (auto framebuffer : m_swapChainFramebuffers) {
				vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
			}

			vkDestroyPipeline(logicalDevice, m_graphicsPipeline, nullptr);
			vkDestroyPipelineLayout(logicalDevice, m_pipelineLayout, nullptr);
			vkDestroyRenderPass(logicalDevice, m_renderPass, nullptr);
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
		void Renderer::CreateGraphicsPipeline()
		{
			VkShaderModule vertexShaderModule = CreateShaderModule(std::string(DATA_DIRECTORY) + "Shaders/Vertex/textured_vert.spv"); // TODO eewwwww
			VkShaderModule fragmentShaderModule = CreateShaderModule(std::string(DATA_DIRECTORY) + "Shaders/Fragment/textured_frag.spv");

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
			auto bindingDescription = Vertex::GetBindingDescription();
			auto attributeDescriptions = Vertex::GetAttributeDescriptions();

			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
			vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
			vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

			VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
			inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			inputAssembly.primitiveRestartEnable = VK_FALSE;

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			VkExtent2D const swapChainExtent = m_swapChain.GetExtent();
			viewport.width = (float)swapChainExtent.width;
			viewport.height = (float)swapChainExtent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRect2D scissor{};
			scissor.offset = { 0, 0 };
			scissor.extent = swapChainExtent;

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
			rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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

			VkPipelineDepthStencilStateCreateInfo depthStencil{};
			depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencil.depthTestEnable = VK_TRUE;
			depthStencil.depthWriteEnable = VK_TRUE;
			depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
			depthStencil.depthBoundsTestEnable = VK_FALSE;
			depthStencil.minDepthBounds = 0.0f; // Optional
			depthStencil.maxDepthBounds = 1.0f; // Optional
			depthStencil.stencilTestEnable = VK_FALSE;
			depthStencil.front = {}; // Optional
			depthStencil.back = {}; // Optional

			VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = 1;
			pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
			pipelineLayoutInfo.pushConstantRangeCount = 0;
			pipelineLayoutInfo.pPushConstantRanges = nullptr;

			if (vkCreatePipelineLayout(m_device.GetLogicalDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
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
			pipelineInfo.pDepthStencilState = &depthStencil;

			if (vkCreateGraphicsPipelines(m_device.GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
				throw std::runtime_error("failed to create graphics pipeline!");
			}

			vkDestroyShaderModule(m_device.GetLogicalDevice(), fragmentShaderModule, nullptr);
			vkDestroyShaderModule(m_device.GetLogicalDevice(), vertexShaderModule, nullptr);
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
			if (vkCreateShaderModule(m_device.GetLogicalDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
				throw std::runtime_error("failed to create shader module!");
			}

			return shaderModule;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateRenderPass()
		{
			VkAttachmentDescription colorAttachment{};
			colorAttachment.format = m_swapChain.GetFormat();
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

			VkAttachmentDescription depthAttachment{};
			depthAttachment.format = FindDepthFormat();
			depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentReference depthAttachmentRef{};
			depthAttachmentRef.attachment = 1;
			depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;
			subpass.pDepthStencilAttachment = &depthAttachmentRef;

			VkSubpassDependency dependency{};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			dependency.srcAccessMask = 0;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
			VkRenderPassCreateInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = static_cast<uint32>(attachments.size());
			renderPassInfo.pAttachments = attachments.data();
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 1;
			renderPassInfo.pDependencies = &dependency;


			if (vkCreateRenderPass(m_device.GetLogicalDevice(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
				throw std::runtime_error("failed to create render pass!");
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateFramebuffers()
		{
			auto const& imageViews = m_swapChain.GetImageViews();
			m_swapChainFramebuffers.resize(imageViews.size());

			for (size_t i = 0; i < imageViews.size(); i++) {
				std::array<VkImageView, 2> attachments = {
					imageViews[i],
					m_depthImageView
				};

				VkFramebufferCreateInfo framebufferInfo{};
				framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferInfo.renderPass = m_renderPass;
				framebufferInfo.attachmentCount = static_cast<uint32>(attachments.size());
				framebufferInfo.pAttachments = attachments.data();
				VkExtent2D const swapChainExtent = m_swapChain.GetExtent();
				framebufferInfo.width = swapChainExtent.width;
				framebufferInfo.height = swapChainExtent.height;
				framebufferInfo.layers = 1;

				if (vkCreateFramebuffer(m_device.GetLogicalDevice(), &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS) {
					throw std::runtime_error("failed to create framebuffer!");
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateVertexBuffer()
		{
			std::vector<Vertex> vertices;

			// Triangle one
			vertices.push_back(Vertex({ -0.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }));
			vertices.push_back(Vertex({ 0.0f, -0.5f, 0.0f }, { 1.0f, 0.25f, 0.0f, 1.0f }, { 0.0f, 0.0f }));
			vertices.push_back(Vertex({ 0.5f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }));

			// Triangle two
			vertices.push_back(Vertex({ -0.5f, 0.0f, -0.25f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }));
			vertices.push_back(Vertex({ 0.5f, 0.0f, -0.25f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }));
			vertices.push_back(Vertex({ 0.0f, 0.5f, -0.25f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }));


			// Second Quad
			// Triangle one
			vertices.push_back(Vertex({ -0.5f, 0.0f, 0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }));
			vertices.push_back(Vertex({ 0.0f, -0.5f, 0.5f }, { 1.0f, 0.25f, 0.0f, 1.0f }, { 0.0f, 0.0f }));
			vertices.push_back(Vertex({ 0.5f, 0.0f, 0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }));

			// Triangle two
			vertices.push_back(Vertex({ -0.5f, 0.0f, 0.25f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }));
			vertices.push_back(Vertex({ 0.5f, 0.0f, 0.25f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }));
			vertices.push_back(Vertex({ 0.0f, 0.5f, 0.25f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }));


			Mesh diamond(vertices);

			VkBufferCreateInfo bufferInfo{}; // TODO move inside Mesh
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = sizeof(Vertex) * diamond.GetVertexCount();
			bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			CreateBuffer(bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_vertexBuffer, m_vertexBufferMemory);

			VkDevice const logicalDevice = m_device.GetLogicalDevice();
			void* data;
			vkMapMemory(logicalDevice, m_vertexBufferMemory, 0, bufferInfo.size, 0, &data);
			memcpy(data, vertices.data(), (size_t)bufferInfo.size);
			vkUnmapMemory(logicalDevice, m_vertexBufferMemory);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateUniformBuffers()
		{
			size_t const imageViewCount = m_swapChain.GetImageViews().size();
			m_uniformBuffers.resize(imageViewCount);
			m_uniformBuffersMemory.resize(imageViewCount);

			VkBufferCreateInfo bufferInfo{}; // TODO move inside Mesh
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = sizeof(GenericUniformBufferObject);
			bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			for (size_t i = 0; i < imageViewCount; i++) {
				CreateBuffer(bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniformBuffers[i], m_uniformBuffersMemory[i]);
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateDescriptorPool()
		{
			uint32 const imageViewCount = static_cast<uint32>(m_swapChain.GetImageViews().size());

			std::array<VkDescriptorPoolSize, 2> poolSizes{};
			poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			poolSizes[0].descriptorCount = imageViewCount;
			poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			poolSizes[1].descriptorCount = imageViewCount;

			VkDescriptorPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.poolSizeCount = static_cast<uint32>(poolSizes.size());;
			poolInfo.pPoolSizes = poolSizes.data();
			
			poolInfo.maxSets = imageViewCount;

			VkDevice const logicalDevice = m_device.GetLogicalDevice();
			if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
				throw std::runtime_error("failed to create descriptor pool!");
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateDescriptorSets()
		{
			uint32 const descriptorCount = static_cast<uint32>(m_swapChain.GetImageViews().size());
			std::vector<VkDescriptorSetLayout> layouts(descriptorCount, m_descriptorSetLayout);
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = m_descriptorPool;
			allocInfo.descriptorSetCount = descriptorCount;
			allocInfo.pSetLayouts = layouts.data();

			m_descriptorSets.resize(descriptorCount);
			VkDevice const logicalDevice = m_device.GetLogicalDevice();
			if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
				throw std::runtime_error("failed to allocate descriptor sets!");
			}

			for (size_t i = 0; i < descriptorCount; i++) {
				VkDescriptorBufferInfo bufferInfo{};
				bufferInfo.buffer = m_uniformBuffers[i];
				bufferInfo.offset = 0;
				bufferInfo.range = sizeof(GenericUniformBufferObject);

				VkDescriptorImageInfo imageInfo{};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = m_textureImageView;
				imageInfo.sampler = m_textureSampler;

				VkWriteDescriptorSet descriptorWrite{};
				std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

				descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[0].dstSet = m_descriptorSets[i];
				descriptorWrites[0].dstBinding = 0;
				descriptorWrites[0].dstArrayElement = 0;
				descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrites[0].descriptorCount = 1;
				descriptorWrites[0].pBufferInfo = &bufferInfo;

				descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[1].dstSet = m_descriptorSets[i];
				descriptorWrites[1].dstBinding = 1;
				descriptorWrites[1].dstArrayElement = 0;
				descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrites[1].descriptorCount = 1;
				descriptorWrites[1].pImageInfo = &imageInfo;

				vkUpdateDescriptorSets(logicalDevice, static_cast<uint32>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateCommandPool()
		{
			VkCommandPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.queueFamilyIndex = m_device.GetQueueFamilies().m_graphicsFamily.value();
			poolInfo.flags = 0;

			if (vkCreateCommandPool(m_device.GetLogicalDevice(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
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

			if (vkAllocateCommandBuffers(m_device.GetLogicalDevice(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
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
				renderPassInfo.renderArea.extent = m_swapChain.GetExtent();

				std::array<VkClearValue, 2> clearValues{};
				clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
				clearValues[1].depthStencil = { 1.0f, 0 };

				renderPassInfo.clearValueCount = static_cast<uint32>(clearValues.size());
				renderPassInfo.pClearValues = clearValues.data();

				vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

				VkBuffer vertexBuffers[] = { m_vertexBuffer }; // TODO link to mesh
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(m_commandBuffers[i], 0, 1, vertexBuffers, offsets);
				vkCmdBindDescriptorSets(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[i], 0, nullptr);
				vkCmdDraw(m_commandBuffers[i], static_cast<uint32_t>(12), 1, 0, 0); // TODO link to mesh

				vkCmdEndRenderPass(m_commandBuffers[i]);

				if (vkEndCommandBuffer(m_commandBuffers[i]) != VK_SUCCESS) {
					throw std::runtime_error("failed to record command buffer!");
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkCommandBuffer Renderer::BeginSingleTimeCommands()
		{
			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = m_commandPool;
			allocInfo.commandBufferCount = 1;

			VkCommandBuffer commandBuffer;
			vkAllocateCommandBuffers(m_device.GetLogicalDevice(), &allocInfo, &commandBuffer);

			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(commandBuffer, &beginInfo);

			return commandBuffer;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::EndSingleTimeCommands(VkCommandBuffer _commandBuffer)
		{
			vkEndCommandBuffer(_commandBuffer);

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &_commandBuffer;

			vkQueueSubmit(m_device.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
			vkQueueWaitIdle(m_device.GetGraphicsQueue());

			vkFreeCommandBuffers(m_device.GetLogicalDevice(), m_commandPool, 1, &_commandBuffer);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CopyBuffer(VkBuffer _sourceBuffer, VkBuffer _destBuffer, VkDeviceSize _size)
		{
			VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

			VkBufferCopy copyRegion{};
			copyRegion.size = _size;
			vkCmdCopyBuffer(commandBuffer, _sourceBuffer, _destBuffer, 1, &copyRegion);

			EndSingleTimeCommands(commandBuffer);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CopyBufferToImage(VkBuffer _buffer, VkImage _image, uint32 _width, uint32 _height)
		{
			VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

			VkBufferImageCopy region{};
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;

			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;

			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = {
				_width,
				_height,
				1
			};

			vkCmdCopyBufferToImage(
				commandBuffer,
				_buffer,
				_image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&region
			);

			EndSingleTimeCommands(commandBuffer);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateSemaphores()
		{
			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			if (vkCreateSemaphore(m_device.GetLogicalDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphore) != VK_SUCCESS ||
				vkCreateSemaphore(m_device.GetLogicalDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphore) != VK_SUCCESS) {

				throw std::runtime_error("failed to create semaphores!");
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateTextureImage()
		{
			int texWidth = 0;
			int texHeight = 0;
			int texChannels = 0;

			std::string const textureFile = (std::string(DATA_DIRECTORY) + "Textures/oscar.png");
			stbi_uc* pixels = stbi_load(textureFile.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha); // TODO eewwwww
			VkDeviceSize imageSize = static_cast<uint64>(texWidth) * static_cast<uint64>(texHeight) * 4u;

			if (!pixels) {
				throw std::runtime_error("failed to load texture image!");
			}

			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = imageSize;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VkBuffer stagingBuffer; // Currently just used for textures
			VkDeviceMemory stagingBufferMemory;

			CreateBuffer(bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

			VkDevice const device = m_device.GetLogicalDevice();
			void* data;
			vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data); // TODO destroy meeeee
			memcpy(data, pixels, static_cast<size_t>(imageSize));
			vkUnmapMemory(device, stagingBufferMemory);

			stbi_image_free(pixels);

			CreateImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_textureImage, m_textureImageMemory);

			TransitionImageLayout(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			CopyBufferToImage(stagingBuffer, m_textureImage, static_cast<uint32>(texWidth), static_cast<uint32>(texHeight));
			TransitionImageLayout(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			vkDestroyBuffer(device, stagingBuffer, nullptr);
			vkFreeMemory(device, stagingBufferMemory, nullptr);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateTextureImageView()
		{
			m_textureImageView = CreateImageView(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateTextureSampler()
		{
			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.anisotropyEnable = VK_TRUE;

			VkPhysicalDeviceProperties properties{};
			vkGetPhysicalDeviceProperties(m_device.GetPhysicalDevice(), &properties);
			samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = 0.0f;

			if (vkCreateSampler(m_device.GetLogicalDevice(), &samplerInfo, nullptr, &m_textureSampler) != VK_SUCCESS) {
				throw std::runtime_error("failed to create texture sampler!");
			}


		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateImage(uint32 _width, uint32 _height, VkFormat _format, VkImageTiling _tiling, VkImageUsageFlags _usage, VkMemoryPropertyFlags _properties, VkImage& _image, VkDeviceMemory& _imageMemory) const
		{

			VkImageCreateInfo imageInfo{};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent.width = static_cast<uint32_t>(_width);
			imageInfo.extent.height = static_cast<uint32_t>(_height);
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.format = _format;
			imageInfo.tiling = _tiling;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = _usage;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.flags = 0; // Optional


			VkDevice const device = m_device.GetLogicalDevice();
			if (vkCreateImage(device, &imageInfo, nullptr, &_image) != VK_SUCCESS) {
				throw std::runtime_error("failed to create image!");
			}

			VkMemoryRequirements memRequirements;
			vkGetImageMemoryRequirements(device, _image, &memRequirements);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = m_device.FindMemoryType(memRequirements.memoryTypeBits, _properties);

			if (vkAllocateMemory(device, &allocInfo, nullptr, &_imageMemory) != VK_SUCCESS) {
				throw std::runtime_error("failed to allocate image memory!");
			}

			vkBindImageMemory(device, _image, _imageMemory, 0);

		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkImageView Renderer::CreateImageView(VkImage _image, VkFormat _format, VkImageAspectFlags _aspectFlags)
		{
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = _image;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = _format;
			viewInfo.subresourceRange.aspectMask = _aspectFlags;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;

			VkImageView imageView;
			if (vkCreateImageView(m_device.GetLogicalDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
				throw std::runtime_error("failed to create texture image view!");
			}

			return imageView;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::TransitionImageLayout(VkImage _image, VkFormat _format, VkImageLayout _oldLayout, VkImageLayout _newLayout)
		{
			VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = _oldLayout;
			barrier.newLayout = _newLayout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			barrier.image = _image;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			barrier.srcAccessMask = 0; // TODO - mip maps
			barrier.dstAccessMask = 0; // TODO

			VkPipelineStageFlags sourceStage;
			VkPipelineStageFlags destinationStage;

			if (_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && _newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

				sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			}
			else if (_oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && _newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			}
			else {
				throw std::invalid_argument("unsupported layout transition!");
			}

			vkCmdPipelineBarrier(
				commandBuffer,
				sourceStage, destinationStage,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);

			EndSingleTimeCommands(commandBuffer);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateDepthResources()
		{
			VkFormat const depthFormat = FindDepthFormat();
			VkExtent2D const swapChainExtent = m_swapChain.GetExtent();
			CreateImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage, m_depthImageMemory);
			m_depthImageView = CreateImageView(m_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkFormat Renderer::FindDepthFormat() const
		{
			return FindSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		bool Renderer::HasStencilComponent(VkFormat _format) const
		{
			return ((_format == VK_FORMAT_D32_SFLOAT_S8_UINT) || (_format == VK_FORMAT_D24_UNORM_S8_UINT));
		}

		//////////////////////////////////////////////////////////////////////////////////////
		VkFormat Renderer::FindSupportedFormat(const std::vector<VkFormat>& _candidates, VkImageTiling _tiling, VkFormatFeatureFlags _features) const
		{
			for (VkFormat format : _candidates) {
				VkFormatProperties props;
				vkGetPhysicalDeviceFormatProperties(m_device.GetPhysicalDevice(), format, &props);

				if (_tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & _features) == _features) {
					return format;
				}
				else if (_tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & _features) == _features) {
					return format;
				}
			}

			throw std::runtime_error("failed to find supported format!");
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateBuffer(VkBufferCreateInfo _createInfo, VkMemoryPropertyFlags _properties, VkBuffer& o_buffer, VkDeviceMemory& o_bufferMemory)
		{
			VkDevice const logicalDevice = m_device.GetLogicalDevice();
			if (vkCreateBuffer(logicalDevice, &_createInfo, nullptr, &o_buffer) != VK_SUCCESS) {
				throw std::runtime_error("failed to create buffer!");
			}

			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements(logicalDevice, o_buffer, &memRequirements);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = m_device.FindMemoryType(memRequirements.memoryTypeBits, _properties);

			if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &o_bufferMemory) != VK_SUCCESS) {
				throw std::runtime_error("failed to allocate buffer memory!");
			}

			vkBindBufferMemory(logicalDevice, o_buffer, o_bufferMemory, 0);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateInstance()
		{
			if (m_validation.UseValidationLayers() && !m_validation.ValidationLayersSupported()) {
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
			if (m_validation.UseValidationLayers()) {
				auto const& validationLayers = m_validation.GetValidationLayers();
				createInfo.enabledLayerCount = static_cast<uint32>(validationLayers.size());
				createInfo.ppEnabledLayerNames = validationLayers.data();
				VkDebugUtilsMessengerCreateInfoEXT const debugCreateInfo = m_validation.GetDebugUtilsCreateInfo();
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

			if (m_validation.UseValidationLayers()) {
				extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			}

			return extensions;
		}
	}
}