#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>

#include <Singularity.Core/CoreDeclare.h>

namespace Singularity
{
	namespace Render
	{
		struct Vertex
		{
			Vertex() {}
			Vertex(glm::vec3 _position) : m_position(_position) {}
			Vertex(glm::vec3 _position, glm::vec4 _colour) : m_position(_position), m_colour(_colour) {}
			Vertex(glm::vec3 _position, glm::vec4 _colour, glm::vec2 _uv) : m_position(_position), m_colour(_colour), m_uv(_uv) {}

			glm::vec3 m_position;
			glm::vec4 m_colour;
			glm::vec2 m_uv;
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

