#pragma once

namespace nether
{
	// Common values used through out the project.
	constexpr uint32_t INVALID_INDEX_UINT32 = 0xFFFF'FFFF;
	constexpr uint64_t INVALID_INDEX_UINT64 = 0XFFFF'FFFF'FFFF'FFFF;

	// Data types to be used throughout the project.
	struct Uint2
	{
		uint32_t x{};
		uint32_t y{};

		auto operator<=>(const Uint2& other) const = default;
	};
}