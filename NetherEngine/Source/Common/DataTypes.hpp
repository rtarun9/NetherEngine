#pragma once

namespace nether
{
	// Data types to be used throughout the project.
	struct Uint2
	{
		uint32_t x{};
		uint32_t y{};

		auto operator<=>(const Uint2& other) const = default;
	};
}