#pragma once

#include <vulkan/vulkan.hpp>

#include <Singularity.Core/CoreDeclare.h>
#include <Singularity.Render/Buffer.h>
#include <Singularity.Render/GenericUniformBufferObject.h>
#include <Singularity.Render/Mesh.h>
#include <Singularity.Render/Texture.h>

namespace Singularity
{
	namespace Render
	{

		struct GenericUniformBufferObject;
		class Buffer;
		class Mesh;
		class Renderer;
		class Texture;

		class RenderObject
		{
		public:
			RenderObject(Renderer& _renderer) : m_renderer(_renderer) {}

			void SetMesh(Mesh const* _mesh) { m_meshRef = _mesh; }
			void SetTexture(Texture const* _texture) { m_textureRef = _texture; }

			void CreateDescriptorSets();
			void UpdateUniformBuffer(uint64 _imageIndex);
			void WriteDrawToCommandBuffer(VkCommandBuffer _commandBuffer, uint64 _imageIndex);
			void SetupUniform();

		private:
			Renderer& m_renderer;

			Mesh const* m_meshRef = nullptr;
			Texture const* m_textureRef = nullptr;

			std::vector<VkDescriptorSet> m_descriptorSets;

			GenericUniformBufferObject m_uniform;
			std::vector<Buffer>* m_uniformBuffersRef = nullptr; // TODO abstract uniform buffer into something less horrible
			VkDeviceSize m_uniformBufferOffset = 0u;
		};
	}
}

