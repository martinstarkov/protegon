#pragma once

#include <bit>
#include <cstdint>
#include <magic_enum/magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "core/log.h"
#include "serialization/json/json.h"

namespace ptgn {

enum class FontStyle : std::uint32_t {
	Normal		  = 0,
	Bold		  = 1 << 0,
	Italic		  = 1 << 1,
	Underline	  = 1 << 2,
	Strikethrough = 1 << 3,
};

constexpr FontStyle operator|(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(std::to_underlying(a) | std::to_underlying(b));
}

constexpr FontStyle& operator|=(FontStyle& a, FontStyle b) {
	a = a | b;
	return a;
}

constexpr bool HasFlag(FontStyle value, FontStyle flag) {
	return (std::to_underlying(value) & std::to_underlying(flag)) != 0u;
}

inline std::ostream& operator<<(std::ostream& os, FontStyle style) {
	using enum FontStyle;

	if (style == Normal) {
		return os << "Normal";
	}

	bool first = true;

	auto print = [&](FontStyle flag, const char* name) {
		if (HasFlag(style, flag)) {
			if (!first) {
				os << " | ";
			}
			os << name;
			first = false;
		}
	};

	print(Bold, "Bold");
	print(Italic, "Italic");
	print(Underline, "Underline");
	print(Strikethrough, "Strikethrough");

	if (first) {
		// No known flags matched
		PTGN_ERROR("Unknown FontStyle: ", std::to_underlying(style));
	}

	return os;
}

inline void to_json(json& j, FontStyle style) {
	if (auto name{ magic_enum::enum_name(style) }; !name.empty()) {
		j = name;
		return;
	}

	auto remaining_bits{ std::to_underlying(style) };

	j = json::array();

	for (FontStyle flag : magic_enum::enum_values<FontStyle>()) {
		auto flag_bits{ std::to_underlying(flag) };

		// Ignore Normal/zero and declared composite values.
		if (flag_bits == 0 || !std::has_single_bit(flag_bits)) {
			continue;
		}

		if ((remaining_bits & flag_bits) != flag_bits) {
			continue;
		}

		auto name{ magic_enum::enum_name(flag) };

		if (name.empty()) {
			continue;
		}

		j.push_back(name);
		remaining_bits &= ~flag_bits;
	}

	if (remaining_bits != 0) {
		PTGN_ERROR("FontStyle contains unknown flag bits: ", remaining_bits);
	}
}

inline void from_json(const json& j, FontStyle& style) {
	if (j.is_string()) {
		auto parsed{ magic_enum::enum_cast<FontStyle>(j.get<std::string>()) };

		if (!parsed.has_value()) {
			PTGN_ERROR("Unknown FontStyle value: ", j.dump());
		}

		style = parsed.value();
		return;
	}

	if (!j.is_array()) {
		PTGN_ERROR("FontStyle must be an enum name or an array of enum names: ", j.dump());
	}

	FontStyle combined{ FontStyle::Normal };

	for (const auto& value : j) {
		if (!value.is_string()) {
			PTGN_ERROR("FontStyle array values must be strings: ", value.dump());
		}

		auto parsed{ magic_enum::enum_cast<FontStyle>(value.get<std::string>()) };

		if (!parsed.has_value()) {
			PTGN_ERROR("Unknown FontStyle value: ", value.dump());
		}

		combined |= parsed.value();
	}

	style = combined;
}

} // namespace ptgn