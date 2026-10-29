#pragma once

#include <concepts>
#include <istream>
#include <ostream>

namespace ptgn {

template <typename T>
concept StreamWritable = requires(std::ostream& os, const T& value) {
	{ os << value } -> std::same_as<std::ostream&>;
};

template <typename T>
concept StreamReadable = requires(std::istream& is, T& value) {
	{ is >> value } -> std::same_as<std::istream&>;
};

template <typename T>
concept Streamable = StreamWritable<T> && StreamReadable<T>;

} // namespace ptgn
