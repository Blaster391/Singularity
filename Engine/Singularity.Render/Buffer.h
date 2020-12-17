#pragma once

#include <vulkan/vulkan.hpp>

#include <Singularity.Core/CoreDeclare.h>

namespace Singularity
{
	namespace Render
	{
		class Renderer;

		class Buffer
		{
		public:
			Buffer(Renderer& _renderer) : m_renderer(_renderer) {}
			~Buffer();

			void CreateBuffer(VkBufferCreateInfo _createInfo, VkMemoryPropertyFlags _properties);
			void CopyBuffer(VkBuffer _destBuffer);
			void CopyBufferToImage(VkImage _image, uint32 _width, uint32 _height);
			void DestroyBuffer();

			VkBuffer GetBuffer() const { return m_buffer; }
			VkDeviceMemory GetBufferMemory() const { return m_bufferMemory; }
			VkDeviceSize GetDeviceSize() const { return m_size; }

		private:
			VkBuffer m_buffer = nullptr; 
			VkDeviceMemory m_bufferMemory = nullptr;
			VkDeviceSize m_size = 0u;

			Renderer& m_renderer;
		};
	}
}

