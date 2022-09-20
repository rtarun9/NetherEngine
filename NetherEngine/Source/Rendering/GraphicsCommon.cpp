#include "Pch.hpp"

#include "GraphicsCommon.hpp"

namespace nether::rendering
{
	void SetName(ID3D12Object* const object, const std::wstring_view name)
	{
		if constexpr (NETHER_DEBUG_BUILD)
		{
			object->SetName(name.data());
		}
	}

	std::vector<D3D12_INPUT_ELEMENT_DESC> ConstructInputElementDesc(const std::span<InputElementDesc> inputElementDesc)
	{
		std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements{};
		inputElements.reserve(inputElementDesc.size());

		for (auto& elementDesc : inputElementDesc)
		{
			elementDesc.inputElementDesc.SemanticName = elementDesc.semanticName.c_str();
			inputElements.push_back(elementDesc.inputElementDesc);
		}

		return std::move(inputElements);
	}
}