#include "MeshLoader.h"

//External
#define TINYOBJLOADER_IMPLEMENTATION 
#include <tinyobj/tiny_obj_loader.h>
#include <iostream>

#include <Singularity.Render/Mesh.h>


namespace Singularity
{
	namespace Render
	{
		//////////////////////////////////////////////////////////////////////////////////////
		Mesh MeshLoader::LoadObj(std::string _file)
		{
			tinyobj::ObjReaderConfig reader_config;


			tinyobj::ObjReader reader;
			if (!reader.ParseFromFile(_file, reader_config)) {
				if (!reader.Error().empty()) {
					throw std::runtime_error("Failed to load obj file: " + _file + ", error: " + reader.Error());
				}
				else
				{
					throw std::runtime_error("Failed to load obj file: " + _file);
				}
			}


			if (!reader.Warning().empty()) {
				std::cout << "TinyObjReader: " << reader.Warning();
			}

			auto& attrib = reader.GetAttrib();
			auto& shapes = reader.GetShapes();
			

			// TODO materials

			if (shapes.size() > 1u)
			{
				std::cout << "TinyObjReader: currently only supports a single shape, " + _file + "contains many";
			}

			if (shapes.empty())
			{
				throw std::runtime_error("TinyObjReader: could not find shape in " + _file);
			}

			std::vector<uint32> indices;
			std::vector<Vertex> vertices;
			
			auto const& shape = shapes.front();
			indices.reserve(shape.mesh.indices.size());
			vertices.reserve(shape.mesh.indices.size());
			// Loop over faces(polygon) // TODO sort this mess out lol
			size_t index_offset = 0;
			for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
			{

				int fv = shape.mesh.num_face_vertices[f];

				// Loop over vertices in the face.
				for (size_t v = 0; v < fv; v++) {
					// access to vertex
					uint32 const indexVal = index_offset + v;
					tinyobj::index_t idx = shape.mesh.indices[indexVal];
					indices.push_back(indexVal);

					glm::vec3 position = { attrib.vertices[3 * idx.vertex_index], attrib.vertices[3 * idx.vertex_index + 1], attrib.vertices[3 * idx.vertex_index + 2] };
					glm::vec4 colour(0,0,0,0);
					glm::vec2 uv(0, 0);
					if (idx.texcoord_index > 0u)
					{
						uv = { attrib.texcoords[2 * idx.texcoord_index], 1.0f -  attrib.texcoords[2 * idx.texcoord_index + 1] }; // (1.0f - coordY) because obj is upside down
					}

					vertices.push_back(Vertex(position, colour, uv));

					
				}
				index_offset += fv;

				// per-face material
				//shapes[s].mesh.material_ids[f];
			}


			return Mesh(vertices, indices);
		}

		////////////////////////////////////////////////////////////////////////////////////////
		//Mesh MeshLoader::LoadObj(std::string _file)
		//{
		//	tinyobj::ObjReaderConfig reader_config;


		//	tinyobj::ObjReader reader;
		//	if (!reader.ParseFromFile(_file, reader_config)) {
		//		if (!reader.Error().empty()) {
		//			throw std::runtime_error("Failed to load obj file: "+ _file + ", error: " + reader.Error());
		//		}
		//		else
		//		{
		//			throw std::runtime_error("Failed to load obj file: " + _file);
		//		}
		//	}


		//	if (!reader.Warning().empty()) {
		//		std::cout << "TinyObjReader: " << reader.Warning();
		//	}

		//	auto& attrib = reader.GetAttrib();
		//	auto& shapes = reader.GetShapes();

		//	std::vector<Vertex> vertices;

		//	uint64 const vertexCount = attrib.vertices.size() / 3;

		//	vertices.reserve(vertexCount);


		//	for (uint64 i = 0; i < vertexCount; ++i)
		//	{
		//		uint64 vertOffset = i * 3;
		//		uint64 uvOffset = i * 2;

		//		glm::vec3 position = { attrib.vertices[vertOffset], attrib.vertices[vertOffset + 1], attrib.vertices[vertOffset + 2] };

		//		glm::vec4 colour(0, 0, 0, 0);
		//		if (vertOffset + 2 < attrib.colors.size())
		//		{
		//			colour = { attrib.colors[vertOffset], attrib.colors[vertOffset + 1], attrib.colors[vertOffset + 2], 1.0f };
		//		}

		//		//glm::vec2 uv(0, 0);
		//		//if (uvOffset + 1 < attrib.texcoords.size())
		//		//{
		//		//	uv = { attrib.texcoords[uvOffset], attrib.texcoords[uvOffset + 1] };
		//		//	//uv = { attrib.texcoords[uvOffset], 1.0f -  attrib.texcoords[uvOffset + 1] }; // might need to flip uvs
		//		//}

		//		vertices.push_back(Vertex(position, colour));
		//	}


		//	// TODO materials

		//	if (shapes.size() > 1u)
		//	{
		//		std::cout << "TinyObjReader: currently only supports a single shape, " + _file + "contains many";
		//	}

		//	if (shapes.empty())
		//	{
		//		throw std::runtime_error("TinyObjReader: could not find shape in " + _file);
		//	}

		//	std::vector<uint32> indices;
		//	auto const& shape = shapes.front();
		//	indices.reserve(shape.mesh.indices.size());
		//	// Loop over faces(polygon)
		//	size_t index_offset = 0;
		//	for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
		//	{

		//		int fv = shape.mesh.num_face_vertices[f];

		//		// Loop over vertices in the face.
		//		for (size_t v = 0; v < fv; v++) {
		//			// access to vertex
		//			tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
		//			indices.push_back(idx.vertex_index);

		//			if (idx.texcoord_index > 0u)
		//			{
		//				vertices[idx.vertex_index].m_uv = { attrib.texcoords[idx.texcoord_index], attrib.texcoords[idx.texcoord_index + 1] };
		//			}

		//			//tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
		//			//tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
		//			//tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
		//			//tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
		//			//tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
		//			//tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
		//			//tinyobj::real_t tx = attrib.texcoords[2 * idx.texcoord_index + 0];
		//			//tinyobj::real_t ty = attrib.texcoords[2 * idx.texcoord_index + 1];
		//			// Optional: vertex colors
		//			// tinyobj::real_t red = attrib.colors[3*idx.vertex_index+0];
		//			// tinyobj::real_t green = attrib.colors[3*idx.vertex_index+1];
		//			// tinyobj::real_t blue = attrib.colors[3*idx.vertex_index+2];
		//		}
		//		index_offset += fv;

		//		// per-face material
		//		//shapes[s].mesh.material_ids[f];
		//	}
		//	

		//	return Mesh(vertices, indices);
		//}
	}
}