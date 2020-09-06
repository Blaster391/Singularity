#pragma once
#include <array>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

#include <Singularity.Core/CoreDeclare.h>

namespace Singularity
{
	namespace Render
	{
		struct Vertex
		{
		public:
			Vertex() {}
			Vertex(glm::vec3 _position) : m_position(_position) {}
			Vertex(glm::vec3 _position, glm::vec4 _colour) : m_position(_position), m_colour(_colour) {}
			Vertex(glm::vec3 _position, glm::vec4 _colour, glm::vec2 _uv) : m_position(_position), m_colour(_colour), m_uv(_uv) {}

			glm::vec3 m_position;
			glm::vec4 m_colour;
			glm::vec2 m_uv;

			static VkVertexInputBindingDescription GetBindingDescription() 
			{
				VkVertexInputBindingDescription bindingDescription;
				bindingDescription.binding = 0;
				bindingDescription.stride = sizeof(Vertex);
				bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

				return bindingDescription;
			}

			static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions() {
				std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions;

				// Position
				attributeDescriptions[0].binding = 0;
				attributeDescriptions[0].location = 0;
				attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
				attributeDescriptions[0].offset = offsetof(Vertex, m_position);

				// Colour
				attributeDescriptions[1].binding = 0;
				attributeDescriptions[1].location = 1;
				attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
				attributeDescriptions[1].offset = offsetof(Vertex, m_colour);

				// UV
				attributeDescriptions[2].binding = 0;
				attributeDescriptions[2].location = 2;
				attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
				attributeDescriptions[2].offset = offsetof(Vertex, m_uv);

				return attributeDescriptions;
			}
		};

		class Mesh
		{
		public:
			Mesh(std::vector<Vertex> const& _vertices);
			Mesh(std::vector<Vertex> const& _vertices, std::vector<uint32> _indices);
			~Mesh();

			std::vector<Vertex> const& GetVertices() const { return m_vertices; }
			std::vector<uint32> const& GetIndices() const { return m_indices; }

			uint32 GetVertexCount() const { return static_cast<uint32>(m_vertices.size()); }
			uint32 GetIndexCount() const { return static_cast<uint32>(m_indices.size()); }
		private:

			std::vector<Vertex> const m_vertices;
			std::vector<uint32> const m_indices;
		};
	}
}

