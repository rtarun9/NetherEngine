#pragma once

#ifdef NETHER_DEBUG
static constexpr bool NETHER_DEBUG_MODE = true;
#else
static constexpr bool NETHER_DEBUG_MODE = false;
#endif

// STL includes.
#include <iostream>
#include <string>
#include <string_view>
#include <exception>
#include <stdexcept>
#include <cstdint>
#include <format>
#include <span>
#include <source_location>
#include <fstream>
#include <ranges>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <array>
#include <unordered_map>
#include <chrono>

// Windows, DirectX12 and DXGI includes.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <wrl.h>
#include <d3dx12.h>
#include <d3d12shader.h>

// Math library includes.
#include <DirectXMath.h>

// Global aliases.
template <typename T> using Comptr = Microsoft::WRL::ComPtr<T>;
namespace math = DirectX;

// Custom global includes.
#include "NetherEngine/Utils.hpp"
#include "NetherEngine/Types.hpp"
