#pragma once

namespace nether
{
	struct Uint2
	{
		uint32_t x{};
		uint32_t y{};

		auto operator<=>(const Uint2& other) const = default;
	};

	struct Vertex
	{
		DirectX::XMFLOAT3 position{};
		DirectX::XMFLOAT2 textureCoord{};
		DirectX::XMFLOAT3 normal{};
	};
}