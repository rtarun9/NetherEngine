#pragma once

#include "Resources.hpp"

namespace nether::rendering
{
	// Graphics - rendering related common value's and function's to be used throughout the project.
	static constexpr uint32_t NUMBER_OF_FRAMES = 3u;
	static constexpr DXGI_FORMAT SWAP_CHAIN_BACK_BUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;

	void SetName(ID3D12Object* const object, const std::wstring_view name);

	std::vector<D3D12_INPUT_ELEMENT_DESC> ConstructInputElementDesc(const std::span<InputElementDesc> inputElementDesc);
}