#pragma once

namespace nether::utils
{
	std::wstring StringToWString(const std::string_view inputString);
	std::string WStringToString(const std::wstring_view inputWString);
	std::string HresultToString(const HRESULT hr);

	void ErrorMessage(const std::wstring_view message);
	void ErrorMessage(const std::string_view message);

	void ThrowIfFailed(const HRESULT hr);

	void SetName(ID3D12Object* const object, const std::wstring_view name);
}