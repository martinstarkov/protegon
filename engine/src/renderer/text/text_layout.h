#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"
#include "renderer/text/text_glyph.h"
#include "renderer/text/text_style.h"
#include "serialization/serialize.h"

namespace ptgn {

enum class HorizontalAlign : std::uint8_t {
	Left,
	Center,
	Right,
	Justify,
};
PTGN_SERIALIZE_ENUM(HorizontalAlign)

enum class VerticalAlign : std::uint8_t {
	Top,
	Center,
	Bottom,
};
PTGN_SERIALIZE_ENUM(VerticalAlign)

enum class WrapMode : std::uint8_t {
	None,
	Word,
	Character,
};
PTGN_SERIALIZE_ENUM(WrapMode)

enum class OverflowMode : std::uint8_t {
	Overflow,
	/// @brief Hide any glyph not fully inside the rect.
	Clip,
	/// @brief Hide glyphs only when fully outside the rect.
	ClipPartial,
	Ellipsis,
	ScaleToFit,
};
PTGN_SERIALIZE_ENUM(OverflowMode)

struct ShrinkScale {
	float min{ 0.25f };
	float max{ 1.0f };

	constexpr bool operator==(const ShrinkScale& o) const {
		return NearlyEqual(min, o.min) && NearlyEqual(max, o.max);
	}
	PTGN_SERIALIZE(ShrinkScale, min, max)
};

struct TextLayoutStyle {
	HorizontalAlign horizontal_align{ HorizontalAlign::Left };
	VerticalAlign vertical_align{ VerticalAlign::Top };
	WrapMode wrap_mode{ WrapMode::None };
	OverflowMode overflow_mode{ OverflowMode::Overflow };

	/// @brief If true, consecutive whitespace characters will be collapsed into a single space.
	bool collapse_spaces{ false };

	/// @brief If true, the last line of text will be justified to fill the width of the box.
	bool justify_last_line{ false };

	/// @brief Only used by WrapMode::Word.
	/// If true, move the word to a new line, then split it across lines if it cannot fit on an
	/// empty line. If false, move the word to a new line, then allow it to overflow if it is still
	/// too wide.
	bool allow_word_break_in_overflow{ true };

	/// @brief Only used by WrapMode::Character.
	/// If true, inserts a hyphen at the end of a line when a word is split across lines.
	bool insert_hyphen_on_split{ true };

	/// @brief Only used by WrapMode::Character.
	/// If true, if a word is split and only a single letter remains on the current line, the
	/// entire word is moved to the next line instead.
	bool prevent_single_letter_split{ true };

	/// @brief Only used by WrapMode::Character.
	/// If true, if a word is split and fewer than three letters remain on the next line, the
	/// entire word is moved to the next line instead.
	bool require_three_letter_remainder{ true };

	std::size_t max_lines{ 0 };

	ShrinkScale shrink_scale;

	constexpr bool operator==(const TextLayoutStyle&) const = default;

	PTGN_SERIALIZE(
		TextLayoutStyle, horizontal_align, vertical_align, wrap_mode, overflow_mode,
		collapse_spaces, justify_last_line, allow_word_break_in_overflow, insert_hyphen_on_split,
		prevent_single_letter_split, require_three_letter_remainder, max_lines, shrink_scale
	)
};

struct TextBox {
	Rect rect;
	TextLayoutStyle style;

	constexpr bool operator==(const TextBox&) const = default;

	PTGN_SERIALIZE(TextBox, rect, style)
};

struct LineLayout {
	std::size_t glyph_begin{ 0 };
	std::size_t glyph_end{ 0 };

	V2_float size;

	constexpr bool operator==(const LineLayout&) const = default;
};

struct TextMeasurement {
	V2_float size;
	float first_line_height{ 0.0f };
	float max_line_width{ 0.0f };
	std::size_t line_count{ 0 };
	bool truncated{ false };
	float used_shrink_scale{ 1.0f };
};

struct TextBatchStyle {
	impl::TextureId texture{ 0 };
	DistanceFieldStyle sdf;

	constexpr bool operator==(const TextBatchStyle&) const = default;
};

enum class TextDecorationType : std::uint8_t {
	Underline,
	Strikethrough,
};

struct TextDecoration {
	TextDecorationType type{ TextDecorationType::Underline };
	Rect rect;
	Color color{ color::White };

	std::size_t source_run_index{ 0 };
	std::size_t line_index{ 0 };

	bool visible{ true };
};

enum class TextClipMode : std::uint8_t {
	None,
	Clip,
	ClipPartial,
};
PTGN_SERIALIZE_ENUM(TextClipMode)

struct TextLayout {
	std::vector<Glyph> glyphs;
	std::vector<TextDecoration> decorations;
	std::vector<LineLayout> lines;

	/// @brief One style per StyledText run. Glyph::source_run_index indexes this.
	std::vector<TextBatchStyle> batch_styles;

	V2_float measured_size;
	V2_float content_offset;
	float used_shrink_scale{ 1.0f };

	bool clipped{ false };
	bool ellipsized{ false };
	bool truncated_by_max_lines{ false };

	std::optional<Rect> clip_rect;
	TextClipMode clip_mode{ TextClipMode::None };
	Rect local_box;

	std::size_t hash{ 0 };
};

struct DrawTextRequest {
	const TextLayout& layout;
	Color tint{ color::White };
	Depth depth;
	int entity_id{ -1 };
	std::optional<Rect> clip_rect;
	TextClipMode clip_mode;
	std::size_t reveal_glyph_count{ std::numeric_limits<std::size_t>::max() };
	float time{ 0.0f };

	/// @brief Center of the text in world space. Origin should be accounted for in this
	/// transform.
	Transform transform;

	impl::EffectParams effects;
};

namespace impl {

struct TextDrawBatch {
	TextBatchStyle style;
	bool decoration{ false };
	std::vector<impl::TextureQuad> quads;
};

struct TextReveal {
	std::size_t glyph_count{ std::numeric_limits<std::size_t>::max() };

	PTGN_SERIALIZE_VALUE(TextReveal, glyph_count)
};

struct TextClip {
	std::optional<Rect> rect;
	TextClipMode mode{ TextClipMode::Clip };

	PTGN_SERIALIZE(TextClip, rect, mode)
};

[[nodiscard]] TextLayout BuildTextLayout(const ResolvedStyledText& styled_text, const TextBox& box);

[[nodiscard]] TextMeasurement MeasureText(
	const ResolvedStyledText& styled_text, const TextBox& box
);

std::vector<TextDrawBatch> BuildTextDrawBatches(const ptgn::DrawTextRequest& request);

[[nodiscard]] bool TextLayoutFitsInBox(const TextLayout& layout, Rect box);

} // namespace impl

} // namespace ptgn

template <>
struct std::hash<ptgn::ShrinkScale> {
	std::size_t operator()(const ptgn::ShrinkScale& scale) const {
		return ptgn::Hash(ptgn::QuantizeUnsigned(scale.min), ptgn::QuantizeUnsigned(scale.max));
	}
};

template <>
struct std::hash<ptgn::TextLayoutStyle> {
	std::size_t operator()(const ptgn::TextLayoutStyle& style) const {
		return ptgn::Hash(
			std::to_underlying(style.horizontal_align), std::to_underlying(style.vertical_align),
			std::to_underlying(style.wrap_mode), std::to_underlying(style.overflow_mode),

			style.collapse_spaces, style.justify_last_line, style.allow_word_break_in_overflow,

			style.insert_hyphen_on_split, style.prevent_single_letter_split,
			style.require_three_letter_remainder,

			style.max_lines, style.shrink_scale
		);
	}
};

template <>
struct std::hash<ptgn::TextBox> {
	std::size_t operator()(const ptgn::TextBox& box) const {
		return ptgn::Hash(box.rect, box.style);
	}
};