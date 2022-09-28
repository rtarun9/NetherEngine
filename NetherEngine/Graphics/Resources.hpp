#pragma once

namespace nether::graphics
{
	// Holds all various resource types (buffers, textures, etc) used for rendering and other related purposes.
	
	enum class BufferUsage : uint8_t
	{
		StructuredBuffer,
		ConstantBuffer,
		IndexBuffer
	};

	struct Buffer
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> resource{};

		uint32_t srvIndex{INVALID_INDEX_UINT32};
		uint32_t cbvIndex{INVALID_INDEX_UINT32};
	};

	// Stride here is basically the size of each component.
	// Buffer size is componenetCount * stride.
	struct BufferCreationDesc
	{
		BufferUsage usage{};
		DXGI_FORMAT format{};
		uint32_t componentCount{};
		size_t stride{};
	};
}


