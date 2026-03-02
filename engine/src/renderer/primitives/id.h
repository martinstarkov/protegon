#pragma once

#include <cstdint>

namespace ptgn::impl {

template <typename Tag>
struct Id {
	std::uint32_t value{ 0 };

	constexpr Id() = default;

	constexpr explicit Id(std::uint32_t v) : value{ v } {}

	constexpr operator std::uint32_t() const {
		return value;
	}
};

} // namespace ptgn::impl