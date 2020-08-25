#include "Mesh.h"
namespace Singularity
{
	namespace Render
	{
		Mesh::Mesh(std::vector<Vertex> const& _vertices)
			: m_vertices(_vertices), m_indices()
		{
		}

		Mesh::Mesh(std::vector<Vertex> const& _vertices, std::vector<uint32> _indices)
			: m_vertices(_vertices), m_indices(_indices)
		{
		}

		Mesh::~Mesh()
		{
		}
	}
}