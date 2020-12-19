#include "Image.h"

#include <Singularity.Render/Renderer.h>

namespace Singularity
{
	namespace Render
	{
		//////////////////////////////////////////////////////////////////////////////////////
		Image::~Image()
		{
			if (m_image)
			{
				DestroyImage();
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Image::CreateImage(uint32 _width, uint32 _height, VkFormat _format, VkImageTiling _tiling, VkImageUsageFlags _usage, VkMemoryPropertyFlags _properties, VkImageAspectFlags _aspectFlags)
		{
			m_imageFormat = _format;

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

			VkDevice const device = m_renderer.GetDevice().GetLogicalDevice();
			if (vkCreateImage(device, &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
				throw std::runtime_error("failed to create image!");
			}

			VkMemoryRequirements memRequirements;
			vkGetImageMemoryRequirements(device, m_image, &memRequirements);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = m_renderer.GetDevice().FindMemoryType(memRequirements.memoryTypeBits, _properties);

			if (vkAllocateMemory(device, &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS) {
				throw std::runtime_error("failed to allocate image memory!");
			}

			vkBindImageMemory(device, m_image, m_imageMemory, 0);

			CreateImageView(_aspectFlags);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Image::TransitionImageLayout(VkImageLayout _oldLayout, VkImageLayout _newLayout)
		{
			VkCommandBuffer commandBuffer = m_renderer.BeginSingleTimeCommands();

			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = _oldLayout;
			barrier.newLayout = _newLayout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

			barrier.image = m_image;
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

			m_renderer.EndSingleTimeCommands(commandBuffer);
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Image::DestroyImage()
		{
			VkDevice const logicalDevice = m_renderer.GetDevice().GetLogicalDevice();
			m_imageFormat = VkFormat::VK_FORMAT_UNDEFINED;
			vkDestroyImageView(logicalDevice, m_imageView, nullptr);
			vkDestroyImage(logicalDevice, m_image, nullptr);
			vkFreeMemory(logicalDevice, m_imageMemory, nullptr);

			m_imageView = nullptr;
			m_image = nullptr;
			m_imageMemory = nullptr;

		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Image::CreateImageView(VkImageAspectFlags _aspectFlags)
		{
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = m_image;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = m_imageFormat;
			viewInfo.subresourceRange.aspectMask = _aspectFlags;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(m_renderer.GetDevice().GetLogicalDevice(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
				throw std::runtime_error("failed to create texture image view!");
			}
		}
	}
}