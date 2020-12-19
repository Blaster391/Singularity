#pragma once

#include <vulkan/vulkan.hpp>

#include <Singularity.Core/CoreDeclare.h>
#include <Singularity.Render/Buffer.h>
#include <Singularity.Render/GenericUniformBufferObject.h>
#include <Singularity.Render/Mesh.h>

namespace Singularity
{
	namespace Render
	{

		struct GenericUniformBufferObject;
		class Mesh;


		class RenderObject
		{
		public:
			void SetMesh(Mesh const* _mesh) { m_meshRef = _mesh; }



		private:
			
			Mesh const* m_meshRef = nullptr;



			GenericUniformBufferObject m_uniform;
			Buffer* m_uniformBufferRef = nullptr;
			VkDeviceSize m_uniformBufferOffset;
		};
	}
}

