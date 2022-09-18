#pragma once

#include "Resources.hpp"

namespace nether
{
	class Engine;
	
	class Model
	{
	public:
		Model(const std::wstring_view modelAssetPath, Engine* engine);

	public:
		std::vector<std::unique_ptr<VertexBuffer>> mVertexBuffers{};
		std::vector<std::unique_ptr<IndexBuffer>> mIndexBuffers{};
	};
}