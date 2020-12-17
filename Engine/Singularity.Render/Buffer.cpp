#include "Buffer.h"

#include <Singularity.Render/Renderer.h>

namespace Singularity
{
	namespace Render
	{
		//////////////////////////////////////////////////////////////////////////////////////
		Buffer::~Buffer()
		{
			if (m_buffer)
			{
				DestroyBuffer();
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Buffer::CreateBuffer(VkBufferCreateInfo _createInfo, VkMemoryPropertyFlags _properties)
		{
			VkDevice const logicalDevice = m_renderer.GetDevice().GetLogicalDevice();
			if (vkCreateBuffer(logicalDevice, &_createInfo, nullptr, &m_buffer) != VK_SUCCESS) {
				throw std::runtime_error("failed to create buffer!");
			}

			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements(logicalDevice, m_buffer, &memRequirements);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = m_renderer.GetDevice().FindMemoryType(memRequirements.memoryTypeBits, _properties);

			if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &m_bufferMemory) != VK_SUCCESS) { // TODO - group model buffers and stuff - vkALlocateMemory is limited https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer (conclusion)
				throw std::runtime_error("failed to allocate buffer memory!");
			}

			vkBindBufferMemory(logicalDevice, m_buffer, m_bufferMemory, 0);

			m_size = _createInfo.size;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Buffer::CopyBuffer(VkBuffer _destBuffer)
		{
			VkCommandBuffer commandBuffer = m_renderer.BeginSingleTimeCommands();

			VkBufferCopy copyRegion{};
			copyRegion.size = m_size;
			vkCmdCopyBuffer(commandBuffer, m_buffer, _destBuffer, 1, &copyRegion);

			m_renderer.EndSingleTimeCommands(commandBuffer);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Buffer::CopyBufferToImage(VkImage _image, uint32 _width, uint32 _height)
		{
			VkCommandBuffer commandBuffer = m_renderer.BeginSingleTimeCommands();

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
				m_buffer,
				_image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&region
			);

			m_renderer.EndSingleTimeCommands(commandBuffer);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Buffer::DestroyBuffer()
		{
			VkDevice const logicalDevice = m_renderer.GetDevice().GetLogicalDevice();
			vkDestroyBuffer(logicalDevice, m_buffer, nullptr);
			vkFreeMemory(logicalDevice, m_bufferMemory, nullptr);

			m_buffer = nullptr;
			m_bufferMemory = nullptr;
			m_size = 0;
		}
	}
}