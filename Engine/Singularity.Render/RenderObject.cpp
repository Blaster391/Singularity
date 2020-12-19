#include "RenderObject.h"

#define GLM_FORCE_RADIANS
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Singularity.Render/Buffer.h>
#include <Singularity.Render/Mesh.h>
#include <Singularity.Render/Texture.h>
#include <Singularity.Render/Renderer.h>

namespace Singularity
{
	namespace Render
	{
		//////////////////////////////////////////////////////////////////////////////////////
		void RenderObject::CreateDescriptorSets()
		{
			uint32 const descriptorCount = static_cast<uint32>(m_renderer.GetSwapChain().GetImageViews().size());
			std::vector<VkDescriptorSetLayout> layouts(descriptorCount, m_renderer.GetDescriptorSetLayout());
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = m_renderer.GetDescriptorPool();
			allocInfo.descriptorSetCount = descriptorCount;
			allocInfo.pSetLayouts = layouts.data();

			m_descriptorSets.resize(descriptorCount);
			VkDevice const logicalDevice = m_renderer.GetDevice().GetLogicalDevice();
			if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
				throw std::runtime_error("failed to allocate descriptor sets!");
			}

			for (size_t i = 0; i < descriptorCount; i++) {
				VkDescriptorBufferInfo bufferInfo{};
				bufferInfo.buffer = (*m_uniformBuffersRef)[i].GetBuffer(); // TODO reference allocator directly maybe???
				bufferInfo.offset = m_uniformBufferOffset;
				bufferInfo.range = sizeof(GenericUniformBufferObject);

				VkDescriptorImageInfo imageInfo{};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = m_textureRef->GetTextureImage().GetImageView(); // TODO optional???
				imageInfo.sampler = m_textureRef->GetTextureSampler();

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
		void RenderObject::UpdateUniformBuffer(uint64 _imageIndex)
		{
			static auto startTime = std::chrono::high_resolution_clock::now();

			auto currentTime = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

			GenericUniformBufferObject ubo{};
			ubo.m_model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

			ubo.m_view = glm::lookAt(glm::vec3(0.0f, 3.0f, 10.0f), glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));

			VkExtent2D const swapChainExtent = m_renderer.GetSwapChain().GetExtent();
			ubo.m_projection = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 1000.0f);

			ubo.m_projection[1][1] *= -1;

			void* data;
			vkMapMemory(m_renderer.GetDevice().GetLogicalDevice(), (*m_uniformBuffersRef)[_imageIndex].GetBufferMemory(), m_uniformBufferOffset, sizeof(ubo), 0, &data);
			memcpy(data, &ubo, sizeof(ubo));
			vkUnmapMemory(m_renderer.GetDevice().GetLogicalDevice(), (*m_uniformBuffersRef)[_imageIndex].GetBufferMemory());
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void RenderObject::WriteDrawToCommandBuffer(VkCommandBuffer _commandBuffer, uint64 _imageIndex)
		{

			if (m_meshRef->UseIndices())
			{
				VkBuffer vertexBuffers[] = { m_meshRef->GetVertexBuffer()->GetBuffer() };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(_commandBuffer, 0, 1, vertexBuffers, offsets);
				vkCmdBindIndexBuffer(_commandBuffer, m_meshRef->GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

				vkCmdBindDescriptorSets(_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_renderer.GetPipelineLayout(), 0, 1, &m_descriptorSets[_imageIndex], 0, nullptr);
				vkCmdDrawIndexed(_commandBuffer, m_meshRef->GetIndexCount(), 1, 0, 0, 0);
			}
			else
			{
				VkBuffer vertexBuffers[] = { m_meshRef->GetVertexBuffer()->GetBuffer() };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(_commandBuffer, 0, 1, vertexBuffers, offsets);

				vkCmdBindDescriptorSets(_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_renderer.GetPipelineLayout(), 0, 1, &m_descriptorSets[_imageIndex], 0, nullptr);
				vkCmdDraw(_commandBuffer, m_meshRef->GetVertexCount(), 1, 0, 0);
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void RenderObject::SetupUniform()
		{
			m_renderer.GetUniformBufferAllocator().Allocate(m_uniformBuffersRef, m_uniformBufferOffset);
		}
	}
}