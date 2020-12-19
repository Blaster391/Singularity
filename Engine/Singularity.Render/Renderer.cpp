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
#include <Singularity.Render/MeshLoader.h>
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
			m_window(_window),
			m_depthImage(*this),
			m_texture(*this),
			m_testMesh(MeshLoader::LoadObj(std::string(DATA_DIRECTORY) + "Models/anky.obj")),
			m_testMesh2(MeshLoader::LoadObj(std::string(DATA_DIRECTORY) + "Models/testSphere.obj"))
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
			VkDevice const device = m_device.GetLogicalDevice();
			vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
	
			uint32 imageIndex;
			VkResult result = vkAcquireNextImageKHR(device, m_swapChain.GetSwapChain(), UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

			// Check if a previous frame is using this image (i.e. there is its fence to wait on)
			if (m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
				vkWaitForFences(device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
			}
			// Mark the image as now being in use by this frame
			m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];

			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				RebuildSwapChain();
				CreateCommandBuffers();
				// return;
			}
			else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
				throw std::runtime_error("failed to acquire swap chain image!");
			}
			
			UpdateUniformBuffers(imageIndex);

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

			VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
			VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];

			VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_currentFrame] };
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;

			vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]);

			if (vkQueueSubmit(m_device.GetGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
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

			result = vkQueuePresentKHR(m_device.GetPresentQueue(), &presentInfo);

			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
				RebuildSwapChain();
				CreateCommandBuffers();
			}
			else if (result != VK_SUCCESS) {
				throw std::runtime_error("failed to present swap chain image!");
			}

			m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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

			CreateUniformBuffers();

			CreatePipeline();
		
			CreateVertexBuffer();
			CreateCommandBuffers();

			CreateSyncObjects();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::Shutdown()
		{
			VkDevice const device = m_device.GetLogicalDevice();
			vkDeviceWaitIdle(device);

			for (uint64 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
				vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
				vkDestroyFence(device, m_inFlightFences[i], nullptr);
			}

			m_testMesh2.Unbuffer();
			m_testMesh.Unbuffer();

			DestroyPipeline();

			for (size_t i = 0; i < m_swapChain.GetImageViews().size(); i++) {
				m_uniformBuffers[i].DestroyBuffer();
			}
			
			vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);

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
			ubo.m_model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

			ubo.m_view = glm::lookAt(glm::vec3(0.0f, 3.0f, 10.0f), glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));

			VkExtent2D const swapChainExtent = m_swapChain.GetExtent();
			ubo.m_projection = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 1000.0f);

			ubo.m_projection[1][1] *= -1;

			void* data;
			vkMapMemory(m_device.GetLogicalDevice(), m_uniformBuffers[_imageIndex].GetBufferMemory(), 0, sizeof(ubo), 0, &data);
			memcpy(data, &ubo, sizeof(ubo));
			vkUnmapMemory(m_device.GetLogicalDevice(), m_uniformBuffers[_imageIndex].GetBufferMemory());
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

			CreateCommandPool(); // TODO ordering and cleanup and not rebuilding stuff i shouldn't
								 // Can not just make this command pool once? Would have to free commands manually tho

			CreateDescriptorPool();

			CreateDepthResources();
			CreateFramebuffers();

			CreateTextureImage();

			CreateDescriptorSets();
			
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::DestroyPipeline()
		{
			VkDevice const logicalDevice = m_device.GetLogicalDevice();

			m_depthImage.DestroyImage();

			m_texture.DestroyTexture();

			vkDestroyCommandPool(logicalDevice, m_commandPool, nullptr);

			vkDestroyDescriptorPool(logicalDevice, m_descriptorPool, nullptr);

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
			pipelineInfo.pDepthStencilState = &depthStencil;
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
					m_depthImage.GetImageView()
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
			//std::vector<Vertex> vertices;

			//// Triangle one
			//vertices.push_back(Vertex({ -0.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }));
			//vertices.push_back(Vertex({ 0.0f, -0.5f, 0.0f }, { 1.0f, 0.25f, 0.0f, 1.0f }, { 0.0f, 0.0f }));
			//vertices.push_back(Vertex({ 0.5f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }));

			//// Triangle two
			//vertices.push_back(Vertex({ -0.5f, 0.0f, -0.25f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }));
			//vertices.push_back(Vertex({ 0.5f, 0.0f, -0.25f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }));
			//vertices.push_back(Vertex({ 0.0f, 0.5f, -0.25f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }));


			//// Second Quad
			//// Triangle one
			//vertices.push_back(Vertex({ -0.5f, 0.0f, 0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }));
			//vertices.push_back(Vertex({ 0.0f, -0.5f, 0.5f }, { 1.0f, 0.25f, 0.0f, 1.0f }, { 0.0f, 0.0f }));
			//vertices.push_back(Vertex({ 0.5f, 0.0f, 0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }));

			//// Triangle two
			//vertices.push_back(Vertex({ -0.5f, 0.0f, 0.25f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }));
			//vertices.push_back(Vertex({ 0.5f, 0.0f, 0.25f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }));
			//vertices.push_back(Vertex({ 0.0f, 0.5f, 0.25f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }));


			//Mesh diamond(vertices);
			// diamond not in use - using obj

			m_testMesh.Buffer(*this);
			m_testMesh2.Buffer(*this);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateUniformBuffers()
		{
			size_t const imageViewCount = m_swapChain.GetImageViews().size();
			m_uniformBuffers.reserve(imageViewCount);

			
			VkBufferCreateInfo bufferInfo{}; // TODO allocate multiple slots for multiple objects
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = sizeof(GenericUniformBufferObject);
			bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			for (size_t i = 0; i < imageViewCount; i++) {
				Buffer& newBuffer = m_uniformBuffers.emplace_back(*this);
				newBuffer.CreateBuffer(bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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
				bufferInfo.buffer = m_uniformBuffers[i].GetBuffer();
				bufferInfo.offset = 0;
				bufferInfo.range = sizeof(GenericUniformBufferObject);

				VkDescriptorImageInfo imageInfo{};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = m_texture.GetTextureImage().GetImageView();
				imageInfo.sampler = m_texture.GetTextureSampler();

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

				std::array<VkClearValue, 2> clearValues{}; // TODO programmable clear colours
				clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
				clearValues[1].depthStencil = { 1.0f, 0 };

				renderPassInfo.clearValueCount = static_cast<uint32>(clearValues.size());
				renderPassInfo.pClearValues = clearValues.data();

				vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

				if (m_testMesh.UseIndices())
				{
					VkBuffer vertexBuffers[] = { m_testMesh.GetVertexBuffer()->GetBuffer() };
					VkDeviceSize offsets[] = { 0 };
					vkCmdBindVertexBuffers(m_commandBuffers[i], 0, 1, vertexBuffers, offsets);
					vkCmdBindIndexBuffer(m_commandBuffers[i], m_testMesh.GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

					vkCmdBindDescriptorSets(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[i], 0, nullptr);
					vkCmdDrawIndexed(m_commandBuffers[i], m_testMesh.GetIndexCount(), 1, 0, 0, 0); 
				}
				else
				{
					VkBuffer vertexBuffers[] = { m_testMesh.GetVertexBuffer()->GetBuffer() };
					VkDeviceSize offsets[] = { 0 };
					vkCmdBindVertexBuffers(m_commandBuffers[i], 0, 1, vertexBuffers, offsets);

					vkCmdBindDescriptorSets(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[i], 0, nullptr);
					vkCmdDraw(m_commandBuffers[i], m_testMesh.GetVertexCount(), 1, 0, 0);
				}


				if (m_testMesh2.UseIndices())
				{
					VkBuffer vertexBuffers[] = { m_testMesh2.GetVertexBuffer()->GetBuffer() };
					VkDeviceSize offsets[] = { 0 };
					vkCmdBindVertexBuffers(m_commandBuffers[i], 0, 1, vertexBuffers, offsets);
					vkCmdBindIndexBuffer(m_commandBuffers[i], m_testMesh2.GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

					vkCmdBindDescriptorSets(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[i], 0, nullptr);
					vkCmdDrawIndexed(m_commandBuffers[i], m_testMesh2.GetIndexCount(), 1, 0, 0, 0);
				}
				else
				{
					VkBuffer vertexBuffers[] = { m_testMesh2.GetVertexBuffer()->GetBuffer() };
					VkDeviceSize offsets[] = { 0 };
					vkCmdBindVertexBuffers(m_commandBuffers[i], 0, 1, vertexBuffers, offsets);

					vkCmdBindDescriptorSets(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets[i], 0, nullptr);
					vkCmdDraw(m_commandBuffers[i], m_testMesh2.GetVertexCount(), 1, 0, 0);
				}



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
		void Renderer::CreateSyncObjects()
		{
			m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
			m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
			m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
			m_imagesInFlight.resize(m_swapChain.GetImageViews().size(), VK_NULL_HANDLE);

			VkDevice const device = m_device.GetLogicalDevice();

			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VkFenceCreateInfo fenceInfo{};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				if ((vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS) ||
					(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS) ||
					(vkCreateFence(device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)){

					throw std::runtime_error("failed to create semaphores for a frame!");
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateTextureImage()
		{
			std::string const textureFile = (std::string(DATA_DIRECTORY) + "Textures/anky.png"); // TODO eewwwww
			m_texture.CreateTexture(textureFile);
		}
		

		//////////////////////////////////////////////////////////////////////////////////////
		void Renderer::CreateDepthResources()
		{
			VkFormat const depthFormat = FindDepthFormat();
			VkExtent2D const swapChainExtent = m_swapChain.GetExtent();
			m_depthImage.CreateImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
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