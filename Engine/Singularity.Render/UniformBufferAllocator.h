#pragma once

#include <vector>

#include <Singularity.Render\Buffer.h>

namespace Singularity
{
	namespace Render
	{
		class Renderer;

		class UniformBufferAllocator
		{
		public:
			UniformBufferAllocator(Renderer& _renderer) : m_renderer(_renderer) {}

			void CreateUniformBuffers();
			void DestroyBuffers();
			void Allocate(std::vector<Buffer>*& o_buffer, VkDeviceSize& o_offset);

		private:
			static uint64 constexpr c_maxUniforms = 100u;

			std::vector<Buffer> m_uniformBuffers;

			Renderer& m_renderer;
			uint64 m_itemCount = 0u;
		};
	}
}

