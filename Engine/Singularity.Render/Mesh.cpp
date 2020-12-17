#include "Mesh.h"

#include <iostream>

#include <Singularity.Render/Renderer.h>

namespace Singularity
{
	namespace Render
	{
		//////////////////////////////////////////////////////////////////////////////////////
		Mesh::~Mesh()
		{
			if (m_buffered)
			{
				Unbuffer();
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Mesh::SetData(std::vector<Vertex> const& _vertices)
		{
			if (m_buffered)
			{
				std::cout << "Error: Setting Data on already buffered mesh!" << std::endl;
			}

			m_vertices = _vertices;
			m_indices.clear();
			m_valid = true;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Mesh::SetData(std::vector<Vertex> const& _vertices, std::vector<uint32> _indices)
		{
			if (m_buffered)
			{
				std::cout << "Error: Setting Data on already buffered mesh!" << std::endl;
			}


			m_vertices = _vertices;
			m_indices = _indices;
			m_valid = true;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Mesh::Buffer(Renderer& _renderer)
		{

			if (!m_valid)
			{
				std::cout << "Error: Attempting to buffer before setting vertex data!" << std::endl;
				return;
			}

			if (m_buffered)
			{
				std::cout << "Error: Attempting to buffer already buffered data!" << std::endl;
				return;
			}

			VkDevice const logicalDevice = _renderer.GetDevice().GetLogicalDevice();

			{
				VkDeviceSize const vertexBufferSize = sizeof(Vertex) * GetVertexCount();
				VkBufferCreateInfo vertexStagingBufferInfo{};
				vertexStagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				vertexStagingBufferInfo.size = vertexBufferSize;
				vertexStagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				vertexStagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				Render::Buffer stagingBuffer(_renderer);
				stagingBuffer.CreateBuffer(vertexStagingBufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

				
				void* data;
				vkMapMemory(logicalDevice, stagingBuffer.GetBufferMemory(), 0, vertexStagingBufferInfo.size, 0, &data);
				memcpy(data, m_vertices.data(), (size_t)vertexStagingBufferInfo.size);
				vkUnmapMemory(logicalDevice, stagingBuffer.GetBufferMemory());

				VkBufferCreateInfo bufferInfo{};
				bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferInfo.size = vertexBufferSize;
				bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
				bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				m_vertexBuffer = new Render::Buffer(_renderer);
				m_vertexBuffer->CreateBuffer(bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

				stagingBuffer.CopyBuffer(m_vertexBuffer->GetBuffer());
				stagingBuffer.DestroyBuffer();
			}

			if (UseIndices())
			{
				VkDeviceSize const indexBufferSize = sizeof(uint32) * GetIndexCount();

				VkBufferCreateInfo indexStagingBufferInfo{};
				indexStagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				indexStagingBufferInfo.size = indexBufferSize;
				indexStagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				indexStagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				Render::Buffer stagingBuffer(_renderer);
				stagingBuffer.CreateBuffer(indexStagingBufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

				void* data;
				vkMapMemory(logicalDevice, stagingBuffer.GetBufferMemory(), 0, indexBufferSize, 0, &data);
				memcpy(data, m_indices.data(), (size_t)indexBufferSize);
				vkUnmapMemory(logicalDevice, stagingBuffer.GetBufferMemory());

				VkBufferCreateInfo bufferInfo{};
				bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferInfo.size = indexBufferSize;
				bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
				bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				m_indexBuffer = new Render::Buffer(_renderer);
				m_indexBuffer->CreateBuffer(bufferInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

				stagingBuffer.CopyBuffer(m_indexBuffer->GetBuffer());
				stagingBuffer.DestroyBuffer();
			}

			m_buffered = true;
		}

		//////////////////////////////////////////////////////////////////////////////////////
		void Mesh::Unbuffer()
		{
			if (m_indexBuffer)
			{
				m_indexBuffer->DestroyBuffer();
				delete m_indexBuffer;
				m_indexBuffer = nullptr;
			}

			if (m_vertexBuffer)
			{
				m_vertexBuffer->DestroyBuffer();
				delete m_vertexBuffer;
				m_vertexBuffer = nullptr;
			}

			m_buffered = false;
		}

	}
}