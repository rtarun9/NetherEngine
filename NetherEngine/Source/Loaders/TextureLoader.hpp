#pragma once

namespace nether::loaders
{
	class TextureLoader
	{
	public:
		TextureLoader(const std::wstring_view texturePath);

	public:
		void* mData{};
		int32_t mTextureWidth{};
		int32_t mTextureHeight{};
		int32_t mTextureChannels{4u};
	};
}