#pragma once

// To prevent excessive use of macro's, setting some constexpr bool values based on build configuration.
#ifdef NETHER_DEBUG
constexpr bool NETHER_DEBUG_BUILD = true;
#else
constexpr bool NETHER_DEBUG_BUILD = false;
#endif

// STL includes.
#include <cstdint>
#include <filesystem>
#include <algorithm>
#include <string_view>
#include <memory>
#include <iostream>
#include <exception>
#include <stdexcept>
#include <ranges>
#include <cassert>
#include <queue>
#include <array>
#include <chrono>
#include <unordered_map>

// Use UNICODE variants of Win32 function's by default.
#ifndef UNICODE
#define UNICODE
#endif

// Exclude rarely used stuff from the windows header.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// DirectX and DXGI includes.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <d3d12shader.h>

// DXC API includes.
#include <dxcapi.h>

// D3D12 helper library.
#include <Core/d3dx12.hpp>

// Helper common includes.
#include "Common/Common.hpp"