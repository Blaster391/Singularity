#include "UniformBufferAllocator.h"

#include <Singularity.Render/GenericUniformBufferObject.h>
#include <Singularity.Render/Renderer.h>

namespace Singularity
{
	namespace Render
	{
		//////////////////////////////////////////////////////////////////////////////////////
		void UniformBufferAllocator::CreateUniformBuffers()
		{
			size_t const imageViewCount = m_renderer.GetSwapChain().GetImageViews().size();
			m_uniformBuffers.reserve(imageViewCount);


			VkBufferCreateInfo bufferInfo{}; // TODO make allocation better
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = sizeof(GenericUniformBufferObject) * c_maxUniforms;
			bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			for (size_t i = 0; i < imageViewCount; i++) {
				Buffer& newBuffer = m_uniformBuffers.emplace_back(m_renderer);
				newBuffer.CreateBuffer(bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void UniformBufferAllocator::DestroyBuffers()
		{
			for (size_t i = 0; i < m_renderer.GetSwapChain().GetImageViews().size(); ++i) {
				m_uniformBuffers[i].DestroyBuffer();
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void UniformBufferAllocator::Allocate(std::vector<Buffer>*& o_buffer, VkDeviceSize& o_offset)
		{
			o_buffer = &m_uniformBuffers;
			o_offset = sizeof(GenericUniformBufferObject) * m_itemCount;
			++m_itemCount;
		}
	}
}