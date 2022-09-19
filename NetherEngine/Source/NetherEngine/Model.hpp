#pragma once

#include "Resources.hpp"

namespace nether
{
	class Device;
	
	class Model
	{
	public:
		Model(const std::wstring_view modelAssetPath, Device* const device);

	public:
		std::vector<VertexBuffer> mVertexBuffers{};
		std::vector<IndexBuffer> mIndexBuffers{};
	};
}