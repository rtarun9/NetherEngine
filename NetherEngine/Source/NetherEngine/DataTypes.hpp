#pragma once

namespace nether
{
	struct Uint2
	{
		uint32_t x{};
		uint32_t y{};

		auto operator<=>(const Uint2& other) const = default;
	};
}