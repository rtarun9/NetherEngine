#pragma once

#include "Resources.hpp"

namespace nether::utils
{
	std::wstring StringToWString(const std::string_view inputString);
	std::string WStringToString(const std::wstring_view inputWString);
	std::string HresultToString(const HRESULT hr);

	void ErrorMessage(const std::wstring_view message);
	void ErrorMessage(const std::string_view message);

	void ThrowIfFailed(const HRESULT hr);

	void SetName(ID3D12Object* const object, const std::wstring_view name);

	std::vector<D3D12_INPUT_ELEMENT_DESC> ConstructInputElementDesc(const std::span<InputElementDesc> inputElementDesc);

	template <typename T>
	static inline constexpr typename std::underlying_type<T>::type EnumClassValue(const T& value)
	{
		return static_cast<std::underlying_type<T>::type>(value);
	}
}