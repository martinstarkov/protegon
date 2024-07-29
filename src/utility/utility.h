#pragma once

#include <algorithm>
#include <array>

namespace ptgn {

template <typename Type, std::size_t... sizes>
auto ConcatenateArrays(const std::array<Type, sizes>&... arrays) {
	std::array<Type, (sizes + ...)> result;
	std::size_t index{};

	((std::copy_n(arrays.begin(), sizes, result.begin() + index), index += sizes), ...);

	return result;
}

} // namespace ptgn
