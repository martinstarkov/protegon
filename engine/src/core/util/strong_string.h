#pragma once

#include <compare>
#include <string>
#include <string_view>
#include <utility>

namespace ptgn {

/// @brief Base for strongly typed string values.
/// The tag type T creates a distinct specialization for each semantic string type, preventing
/// unrelated types from sharing implicit conversions or comparisons through a common String base.
template <typename T>
struct StrongString {
	constexpr StrongString() = default;

	constexpr StrongString(std::string string) : value{ std::move(string) } {} // NOSONAR

	constexpr StrongString(std::string_view string) : value{ string } {}	   // NOSONAR

	constexpr StrongString(const char* string) : value{ string } {}			   // NOSONAR

	explicit constexpr operator bool() const {
		return !value.empty();
	}

	constexpr operator std::string_view() const {							   // NOSONAR
		return value;
	}

	std::string value{};

	constexpr auto operator<=>(const StrongString&) const = default;
};

} // namespace ptgn