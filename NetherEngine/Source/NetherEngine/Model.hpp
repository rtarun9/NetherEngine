#pragma once

#include "Resources.hpp"

namespace tinygltf
{
	class Model;
	class Node;
}

namespace nether
{
	class Device;
	
	class Model
	{
	public:
		Model(const std::wstring_view modelAssetPath, Device* const device);

		void LoadNode(Device* const device,  uint32_t nodeIndex, tinygltf::Model* model);

	public:
		std::vector<VertexBuffer> mVertexBuffers{};
		std::vector<IndexBuffer> mIndexBuffers{};
	};
}