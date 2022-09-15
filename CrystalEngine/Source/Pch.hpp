#pragma once

// STL includes.
#include <cstdint>
#include <algorithm>

// Use UNICODE variants of Win32 function's by default.
#ifndef UNICODE
#define UNICODE
#endif

// Exclude rarely used stuff from the windows header.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>