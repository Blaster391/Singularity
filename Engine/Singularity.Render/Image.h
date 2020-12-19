#pragma once

#include <vulkan/vulkan.hpp>

#include <Singularity.Core/CoreDeclare.h>


namespace Singularity
{
	namespace Render
	{
		class Renderer;

		class Image
		{
		public:
			Image(Renderer& _renderer) : m_renderer(_renderer) {}
			~Image();

			void CreateImage(uint32 _width, uint32 _height, VkFormat _format, VkImageTiling _tiling, VkImageUsageFlags _usage, VkMemoryPropertyFlags _properties, VkImageAspectFlags _aspectFlags);
			void TransitionImageLayout(VkImageLayout _oldLayout, VkImageLayout _newLayout);
			void DestroyImage();

			VkImage GetImage() const { return m_image; }
			VkImageView GetImageView() const { return m_imageView; }

		private:
			Renderer& m_renderer;

			void CreateImageView(VkImageAspectFlags _aspectFlags);

			VkFormat m_imageFormat = VkFormat::VK_FORMAT_UNDEFINED;
			VkImage m_image = nullptr;
			VkImageView m_imageView = nullptr;
			VkDeviceMemory m_imageMemory = nullptr;
		};
	}
}

