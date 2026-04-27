#pragma once

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "runtime/graphics/text/text_effect.h"
#include "runtime/graphics/text/text_style.h"

namespace ptgn {

struct TextLayoutStyle {
	HorizontalAlign horizontal_align{ HorizontalAlign::Left };
	VerticalAlign vertical_align{ VerticalAlign::Top };
	WrapMode wrap_mode{ WrapMode::None };
	OverflowMode overflow_mode{ OverflowMode::Overflow };

	bool collapse_spaces{ false };
	bool justify_last_line{ false };
	bool allow_word_break_in_overflow{ true };

	std::size_t max_lines{ 0 };
	bool ellipsis_on_max_lines{ true };

	float min_shrink_scale{ 0.25f };
	float max_shrink_scale{ 1.0f };
};

struct TextBox {
	Rect rect;
	TextLayoutStyle style;
};

struct LineLayout {
	std::size_t glyph_begin{ 0 };
	std::size_t glyph_end{ 0 };

	V2_float size;
	float baseline_y{ 0.0f };

	std::size_t justify_space_count{ 0 };
	float justify_extra_per_space{ 0.0f };
	bool ends_with_explicit_newline{ false };
};

struct TextMeasurement {
	V2_float size;
	float first_line_height{ 0.0f };
	float max_line_width{ 0.0f };
	std::size_t line_count{ 0 };
	bool truncated{ false };
	float used_shrink_scale{ 1.0f };
};

struct TextLayout {
	std::vector<GlyphInstance> glyphs;
	std::vector<LineLayout> lines;

	V2_float measured_size;
	V2_float content_offset;
	float used_shrink_scale{ 1.0f };

	DistanceFieldStyle batch_style;

	bool clipped{ false };
	bool ellipsized{ false };
	bool truncated_by_max_lines{ false };
};

struct TextLayoutKey {
	std::uint64_t rich_text_hash{ 0 };

	std::uint32_t box_width_q{ 0 };
	std::uint32_t box_height_q{ 0 };
	std::uint32_t min_shrink_scale_q{ 0 };
	std::uint32_t max_shrink_scale_q{ 0 };

	HorizontalAlign horizontal_align{ HorizontalAlign::Left };
	VerticalAlign vertical_align{ VerticalAlign::Top };
	WrapMode wrap_mode{ WrapMode::None };
	OverflowMode overflow_mode{ OverflowMode::Overflow };

	bool collapse_spaces{ false };
	bool justify_last_line{ false };
	bool allow_word_break_in_overflow{ false };
	std::uint32_t max_lines_q{ 0 };
	bool ellipsis_on_max_lines{ false };

	bool operator==(const TextLayoutKey&) const = default;
};

} // namespace ptgn

template <>
struct std::hash<ptgn::TextLayoutKey> {
	std::size_t operator()(const ptgn::TextLayoutKey& key) const {
		return ptgn::Hash(
			key.rich_text_hash, key.box_width_q, key.box_height_q, key.min_shrink_scale_q,
			key.max_shrink_scale_q, std::to_underlying(key.horizontal_align),
			std::to_underlying(key.vertical_align), std::to_underlying(key.wrap_mode),
			std::to_underlying(key.overflow_mode), key.collapse_spaces, key.justify_last_line,
			key.allow_word_break_in_overflow, key.max_lines_q, key.ellipsis_on_max_lines
		);
	}
};

namespace ptgn {

struct TextLayoutRequest {
	StyledText styled_text;
	TextBox box;
};

TextLayoutRequest MakeTextRequest(
	std::string_view content, std::string_view font_key, Rect rect, float scale = 1.0f,
	Color color = color::White, const TextLayoutStyle& layout_style = {}
);

} // namespace ptgn