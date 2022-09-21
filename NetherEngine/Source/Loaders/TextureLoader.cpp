#include "Pch.hpp"

#include "TextureLoader.hpp"

#include "Common/Utils.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace nether::loaders
{
	TextureLoader::TextureLoader(const std::wstring_view texturePath)
	{
		const std::string texturePathStr = utils::WStringToString(texturePath);

		mData = stbi_load(texturePathStr.c_str(), &mTextureWidth, &mTextureHeight, nullptr, mTextureChannels);
	}
}