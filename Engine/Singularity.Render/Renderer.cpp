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

			vkDestroyDescriptorSetLayout(m_device.GetLogicalDevice(), m_descriptorSetLayout, nullptr);

			m_swapChain.Shutdown();

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

			VkDescriptorSetLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = 1;
			layoutInfo.pBindings = &uboLayoutBinding;

			if (vkCreateDescriptorSetLayout(m_device.GetLogicalDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
				throw std::runtime_error("failed to create descriptor set layout!");
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreatePipeline()
		{
			CreateRenderPass();
			CreateGraphicsPipeline();
			CreateFramebuffers();
			CreateVertexBuffer();
			CreateUniformBuffers();
			CreateDescriptorPool();
			CreateDescriptorSets();
			CreateCommandPool();
			CreateCommandBuffers();
			CreateSemaphores();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::DestroyPipeline()
		{
			VkDevice const logicalDevice = m_device.GetLogicalDevice();
			vkDestroySemaphore(logicalDevice, m_renderFinishedSemaphore, nullptr);
			vkDestroySemaphore(logicalDevice, m_imageAvailableSemaphore, nullptr);

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
			VkShaderModule vertexShaderModule = CreateShaderModule(std::string(DATA_DIRECTORY) + "Shaders/Vertex/basic_vert.spv"); // TODO eewwwww
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
				VkImageView attachments[] = {
					imageViews[i]
				};

				VkFramebufferCreateInfo framebufferInfo{};
				framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferInfo.renderPass = m_renderPass;
				framebufferInfo.attachmentCount = 1;
				framebufferInfo.pAttachments = attachments;
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
			vertices.push_back(Vertex({ -0.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }));
			vertices.push_back(Vertex({ 0.0f, -0.5f, 0.0f }, { 1.0f, 0.25f, 0.0f, 1.0f }));
			vertices.push_back(Vertex({ 0.5f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }));

			// Triangle two
			vertices.push_back(Vertex({ -0.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }));
			vertices.push_back(Vertex({ 0.5f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }));
			vertices.push_back(Vertex({ 0.0f, 0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }));
			

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
			VkDescriptorPoolSize poolSize{};
			poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			uint32 const descriptorCount = static_cast<uint32>(m_swapChain.GetImageViews().size());
			poolSize.descriptorCount = descriptorCount;

			VkDescriptorPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.poolSizeCount = 1;
			poolInfo.pPoolSizes = &poolSize;
			poolInfo.maxSets = static_cast<uint32_t>(descriptorCount);

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

				VkWriteDescriptorSet descriptorWrite{};
				descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrite.dstSet = m_descriptorSets[i];
				descriptorWrite.dstBinding = 0;
				descriptorWrite.dstArrayElement = 0;
				descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrite.descriptorCount = 1;
				descriptorWrite.pBufferInfo = &bufferInfo;
				descriptorWrite.pImageInfo = nullptr; // Optional
				descriptorWrite.pTexelBufferView = nullptr; // Optional

				vkUpdateDescriptorSets(logicalDevice, 1, &descriptorWrite, 0, nullptr);

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

				VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
				renderPassInfo.clearValueCount = 1;
				renderPassInfo.pClearValues = &clearColor;

				vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

				VkBuffer vertexBuffers[] = { m_vertexBuffer }; // TODO link to mesh
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(m_commandBuffers[i], 0, 1, vertexBuffers, offsets);
				vkCmdBindDescriptorSets(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[i], 0, nullptr);
				vkCmdDraw(m_commandBuffers[i], static_cast<uint32_t>(6), 1, 0, 0); // TODO link to mesh

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

			if (vkCreateSemaphore(m_device.GetLogicalDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphore) != VK_SUCCESS ||
				vkCreateSemaphore(m_device.GetLogicalDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphore) != VK_SUCCESS) {

				throw std::runtime_error("failed to create semaphores!");
			}
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