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
					throw std::runtime_error("Failed to load obj file: "+ _file + ", error: " + reader.Error());
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

			std::vector<Vertex> vertices;
			vertices.reserve(attrib.vertices.size() / 3);
			for (uint64 i = 0; (i + 2) < attrib.vertices.size(); i = i + 3)
			{
				vertices.push_back(Vertex({ attrib.vertices[i], attrib.vertices[i + 1], attrib.vertices[i + 2] }));
			}


			// TODO materials

			if (shapes.size() > 1u)
			{
				std::cout << "TinyObjReader: currently only supports a single shape, " + _file + "contains many";
			}

			if (shapes.empty())
			{
				throw std::runtime_error("TinyObjReader: could not find shape in " + _file);
			}

			// TODO indicies

			//auto const& shape = shapes.front();
			//// Loop over faces(polygon)
			//size_t index_offset = 0;
			//for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
			//{
			//	int fv = shape.mesh.num_face_vertices[f];

			//	// Loop over vertices in the face.
			//	for (size_t v = 0; v < fv; v++) {
			//		// access to vertex
			//		tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
			//		tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
			//		tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
			//		tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
			//		tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
			//		tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
			//		tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
			//		tinyobj::real_t tx = attrib.texcoords[2 * idx.texcoord_index + 0];
			//		tinyobj::real_t ty = attrib.texcoords[2 * idx.texcoord_index + 1];
			//		// Optional: vertex colors
			//		// tinyobj::real_t red = attrib.colors[3*idx.vertex_index+0];
			//		// tinyobj::real_t green = attrib.colors[3*idx.vertex_index+1];
			//		// tinyobj::real_t blue = attrib.colors[3*idx.vertex_index+2];
			//	}
			//	index_offset += fv;

			//	// per-face material
			//	shapes[s].mesh.material_ids[f];
			//}
			//

			return Mesh(vertices);
		}
	}
}