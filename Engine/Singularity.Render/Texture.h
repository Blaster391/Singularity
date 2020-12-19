#pragma once

#include <string>

#include <Singularity.Render/Image.h>

namespace Singularity
{
	namespace Render
	{
		class Renderer;

		class Texture
		{
		public: 
			Texture(Renderer& _renderer) : m_renderer(_renderer), m_textureImage(_renderer) {}
			~Texture();

			void CreateTexture(std::string _file);
			void DestroyTexture();

			Image const& GetTextureImage() const { return m_textureImage; }
			VkSampler GetTextureSampler() const { return m_textureSampler; }

		private:
			void CreateTextureSampler();

			Renderer& m_renderer;

			Image m_textureImage;
			VkSampler m_textureSampler = nullptr;
		};
	}
}