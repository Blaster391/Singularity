#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <Singularity.Render/Buffer.h>
#include <Singularity.Render/Renderer.h>

namespace Singularity
{
	namespace Render
	{
		//////////////////////////////////////////////////////////////////////////////////////
		Texture::~Texture()
		{
			if (m_textureSampler)
			{
				DestroyTexture();
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Texture::CreateTexture(std::string _file)
		{
			int texWidth = 0;
			int texHeight = 0;
			int texChannels = 0;

			stbi_uc* pixels = stbi_load(_file.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
			VkDeviceSize imageSize = static_cast<uint64>(texWidth) * static_cast<uint64>(texHeight) * 4u;

			if (!pixels) {
				throw std::runtime_error("failed to load texture image!");
			}

			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = imageSize;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;


			Buffer stagingBuffer(m_renderer);
			stagingBuffer.CreateBuffer(bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			VkDevice const device = m_renderer.GetDevice().GetLogicalDevice();
			void* data;
			vkMapMemory(device, stagingBuffer.GetBufferMemory(), 0, imageSize, 0, &data);
			memcpy(data, pixels, static_cast<size_t>(imageSize));
			vkUnmapMemory(device, stagingBuffer.GetBufferMemory());

			stbi_image_free(pixels);

			m_textureImage.CreateImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

			m_textureImage.TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			stagingBuffer.CopyBufferToImage(m_textureImage.GetImage(), static_cast<uint32>(texWidth), static_cast<uint32>(texHeight));
			m_textureImage.TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			stagingBuffer.DestroyBuffer();

			CreateTextureSampler();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Texture::DestroyTexture()
		{
			VkDevice const device = m_renderer.GetDevice().GetLogicalDevice();
			vkDestroySampler(device, m_textureSampler, nullptr);
			m_textureSampler = nullptr;
			m_textureImage.DestroyImage();
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Texture::CreateTextureSampler()
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

			vkGetPhysicalDeviceProperties(m_renderer.GetDevice().GetPhysicalDevice(), &properties);
			samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = 0.0f;

			if (vkCreateSampler(m_renderer.GetDevice().GetLogicalDevice(), &samplerInfo, nullptr, &m_textureSampler) != VK_SUCCESS) {
				throw std::runtime_error("failed to create texture sampler!");
			}
		}
		
	}
}