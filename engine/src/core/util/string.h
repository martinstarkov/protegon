#pragma once

#include <algorithm>
#include <cctype>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>

#include "core/util/concepts_stream.h"

namespace ptgn {

template <StreamWritable T>
[[nodiscard]] std::string ToString(const T& object) {
	std::ostringstream ss;
	ss << object;
	return ss.str();
}

template <StreamWritable... Ts>
[[nodiscard]] std::string ToString(Ts&&... parts) {
	std::ostringstream oss;
	((oss << std::forward<Ts>(parts)), ...);
	return oss.str();
}

/// @param precision The number of decimal places of precision to have in numbers converted to
/// string.
[[nodiscard]] std::string ToString(double value, int precision);

[[nodiscard]] constexpr std::string ToLower(std::string_view str) {
	std::string out;
	out.reserve(str.size());
	std::ranges::transform(str, std::back_inserter(out), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return out;
}

[[nodiscard]] constexpr std::string ToUpper(std::string_view str) {
	std::string out;
	out.reserve(str.size());
	std::ranges::transform(str, std::back_inserter(out), [](unsigned char c) {
		return static_cast<char>(std::toupper(c));
	});
	return out;
}

/// @return True if str begins with the specified prefix.
[[nodiscard]] constexpr bool BeginsWithPrefix(std::string_view str, std::string_view prefix) {
	return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

/// @return True if str ends with the specified suffix.
[[nodiscard]] constexpr bool EndsWithSuffix(std::string_view str, std::string_view suffix) {
	return str.size() >= suffix.size() &&
		   str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

/// @brief Replaces all occurrences of a substring with another substring.
/// @return A new string with all occurrences of `from` replaced by `to`.
[[nodiscard]] constexpr std::string ReplaceAll(
	std::string_view input, std::string_view from, std::string_view to
) {
	std::string result{ input };

	if (from.empty() || from == to) {
		return result;
	}

	std::size_t pos{ 0 };
	while ((pos = result.find(from, pos)) != std::string::npos) {
		result.replace(pos, from.size(), to);
		pos += to.size();
	}

	return result;
}

/// @return New string with just the content inside R"( ... )"
/// This function does not handle delimeters such as R"delim( ... )delim"
[[nodiscard]] constexpr std::string TrimRawStringLiteral(std::string_view content) {
	constexpr std::string_view raw_start{ "R\"(" };
	constexpr std::string_view raw_end{ ")\"" };

	std::size_t start{ content.find(raw_start) };
	std::size_t end{ content.rfind(raw_end) };

	if (start != std::string_view::npos && end != std::string_view::npos &&
		end > start + raw_start.size()) {
		return std::string{
			content.substr(start + raw_start.size(), end - (start + raw_start.size()))
		};
	}

	return std::string{ content };
}

} // namespace ptgn