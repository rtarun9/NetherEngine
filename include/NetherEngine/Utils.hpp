#pragma once

// Global utility / helper function's.

// Reference : https://github.com/turanszkij/WickedEngine/blob/4197633e18f59e3f38c26fa92dc2ed987f646b95/WickedEngine/wiHelper.cpp#L1120.
inline std::wstring stringToWString(const std::string_view inputString)
{
    std::wstring result{};
    const std::string input{inputString};

    const int32_t length = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, NULL, 0);
    if (length > 0)
    {
        result.resize(size_t(length) - 1);
        MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, result.data(), length);
    }

    return result;
}

inline std::string wStringToString(const std::wstring_view inputWString)
{
    std::string result{};
    const std::wstring input{inputWString};

    const int32_t length = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, NULL, 0, NULL, NULL);
    if (length > 0)
    {
        result.resize(size_t(length) - 1);
        WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, result.data(), length, NULL, NULL);
    }

    return result;
}

// Reference : https://github.com/microsoft/DirectX-Graphics-Samples/blob/e5ea2ac7430ce39e6f6d619fd85ae32581931589/Samples/Desktop/D3D12Fullscreen/src/DXSampleHelper.h#L21.
inline std::string hresultToString(const HRESULT hr)
{
    char s_str[64] = {};
    sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
    return std::string(s_str);
}

inline void fatalError(const std::wstring_view message)
{
    ::MessageBoxW(nullptr, message.data(), L"ERROR!", MB_OK | MB_ICONEXCLAMATION);
    const std::string errorMessageStr = wStringToString(message);
    throw std::runtime_error(errorMessageStr);
}

inline void fatalError(const std::string_view message)
{
    ::MessageBoxA(nullptr, message.data(), "ERROR!", MB_OK | MB_ICONEXCLAMATION);
    throw std::runtime_error(message.data());
}

inline void throwIfFailed(const HRESULT hr)
{
    if (FAILED(hr))
    {
        fatalError(hresultToString(hr));
    }
}

inline void setName(ID3D12Object* const object, const std::wstring_view name, const uint32_t index = 0u)
{
    if constexpr (NETHER_DEUBG_MODE)
    {
        if (index == 0)
        {
            throwIfFailed(object->SetName(name.data()));
        }
        else
        {
            const std::wstring objectName = name.data() + std::to_wstring(index);
            throwIfFailed(object->SetName(objectName.data()));
        }
    }
}

inline void debugLog(const std::wstring_view message)
{
    if constexpr (NETHER_DEUBG_MODE)
    {
        std::wcout << "[Debug] : " << message.data() << '\n';
    }
}

[[nodiscard]] inline std::vector<char> readFile(const std::string_view filePath)
{
    using namespace std::string_literals;

    // Specify that the file contains binary content, and go to the end of the file so finding its size is easy.
    std::ifstream file(filePath.data(), std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        fatalError("Failed to read file with path : "s + filePath.data());
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

template <typename T> static inline constexpr typename std::underlying_type<T>::type EnumClassValue(const T& value) { return static_cast<std::underlying_type<T>::type>(value); }
