#include "Utils.hpp"

namespace nether::utils
{
	// Reference : https://github.com/turanszkij/WickedEngine/blob/4197633e18f59e3f38c26fa92dc2ed987f646b95/WickedEngine/wiHelper.cpp#L1120.
	std::wstring StringToWString(const std::string_view inputString)
	{
		std::wstring result{};
		const std::string input{ inputString };

		const int32_t length = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, NULL, 0);
		if (length > 0)
		{
			result.resize(size_t(length) - 1);
			MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, result.data(), length);
		}

		return std::move(result);
	}

	std::string WStringToString(const std::wstring_view inputWString)
	{
		std::string result{};
		const std::wstring input{ inputWString };

		const int32_t length = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, NULL, 0, NULL, NULL);
		if (length > 0)
		{
			result.resize(size_t(length) - 1);
			WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, result.data(), length, NULL, NULL);
		}

		return std::move(result);
	}

	// Reference : https://github.com/microsoft/DirectX-Graphics-Samples/blob/e5ea2ac7430ce39e6f6d619fd85ae32581931589/Samples/Desktop/D3D12Fullscreen/src/DXSampleHelper.h#L21.
	std::string HresultToString(const HRESULT hr)
	{
		char s_str[64] = {};
		sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
		return std::string(s_str);
	}

	void ErrorMessage(const std::wstring_view message)
	{
		::MessageBoxW(nullptr, message.data(), L"ERROR!", MB_OK | MB_ICONEXCLAMATION);
		const std::string errorMessageStr = WStringToString(message);
		throw std::runtime_error(errorMessageStr);
	}

	void ErrorMessage(const std::string_view message)
	{
		::MessageBoxA(nullptr, message.data(), "ERROR!", MB_OK | MB_ICONEXCLAMATION);
		throw std::runtime_error(message.data());
	}

	void ThrowIfFailed(const HRESULT hr)
	{
		if (FAILED(hr))
		{
			ErrorMessage(HresultToString(hr));
		}
	}

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