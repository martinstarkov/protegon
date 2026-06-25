#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
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

	/// @brief Draw a line only when its complete logical bounds are inside the text box.
	Clip,

	/// @brief Draw a line when any part of its logical bounds intersects the text box.
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

	/// @brief Number of space columns between tab stops.
	/// A tab advances to the next multiple of this many spaces.
	std::size_t tab_width{ 4 };

	std::size_t max_lines{ 0 };

	ShrinkScale shrink_scale;

	constexpr bool operator==(const TextLayoutStyle&) const = default;

	PTGN_SERIALIZE(
		TextLayoutStyle, horizontal_align, vertical_align, wrap_mode, overflow_mode,
		collapse_spaces, justify_last_line, allow_word_break_in_overflow, insert_hyphen_on_split,
		prevent_single_letter_split, require_three_letter_remainder, tab_width, max_lines,
		shrink_scale
	)
};

struct TextBox {
	Rect rect;
	TextLayoutStyle style;

	constexpr bool HasWidth() const {
		return rect.GetSize().x > 0.0f;
	}

	constexpr bool HasHeight() const {
		return rect.GetSize().y > 0.0f;
	}

	constexpr bool HasBox() const {
		return HasWidth() || HasHeight();
	}

	constexpr bool HasArea() const {
		return HasWidth() && HasHeight();
	}

	constexpr bool operator==(const TextBox&) const = default;

	PTGN_SERIALIZE(TextBox, rect, style)
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
};

struct LineLayout {
	std::vector<Glyph> glyphs;
	std::vector<TextDecoration> decorations;

	/// @brief Logical line size. The height includes the selected line spacing.
	V2_float size;

	/// @brief Logical line cell after horizontal and vertical alignment.
	Rect bounds;

	float baseline{ 0.0f };

	/// @brief True for a line ended by an explicit newline or by the end of the text.
	/// False for a line produced by automatic wrapping.
	bool paragraph_end{ false };

	constexpr std::size_t GetGlyphCount() const {
		return glyphs.size();
	}
};

struct TextMeasurement {
	V2_float size;
	std::size_t line_count{ 0 };
	bool truncated{ false };
	float used_shrink_scale{ 1.0f };
};

struct TextLayout {
	std::vector<LineLayout> lines;

	/// @brief One style per StyledText run. Glyph::source_run_index indexes this array.
	std::vector<TextBatchStyle> batch_styles;

	/// @brief Final logical size after wrapping, max-line handling, ellipsis, and justification.
	V2_float size;

	float used_shrink_scale{ 1.0f };
	bool wrapped{ false };
	bool truncated{ false };

	/// @brief Number of non-newline source characters after optional whitespace collapsing.
	/// This deliberately excludes synthetic ellipsis and hyphen glyphs.
	std::size_t source_glyph_count{ 0 };

	std::size_t hash{ 0 };

	constexpr const LineLayout& GetLine(std::size_t index) const {
		PTGN_ASSERT(
			index < lines.size(), "Text line index out of range: ", index,
			", line count: ", lines.size()
		);
		return lines[index];
	}

	constexpr float GetLineHeight(std::size_t index = 0) const {
		if (index >= lines.size()) {
			return 0.0f;
		}
		return GetLine(index).size.y;
	}

	Rect GetBounds() const {
		if (lines.empty()) {
			return {};
		}

		Rect bounds{ lines.front().bounds };

		for (auto i{ 1uz }; i < lines.size(); ++i) {
			bounds.min = Min(bounds.min, lines[i].bounds.min);
			bounds.max = Max(bounds.max, lines[i].bounds.max);
		}

		return bounds;
	}

	constexpr std::size_t GetGlyphCount() const {
		return source_glyph_count;
	}

	constexpr std::size_t GetLineCount() const {
		return lines.size();
	}

	std::size_t GetVisibleGlyphCount() const {
		std::size_t count{ 0 };
		for (const auto& line : lines) {
			count += line.glyphs.size();
		}
		return count;
	}
};

enum class TextClipMode : std::uint8_t {
	None,

	/// @brief Keep only lines completely contained by the clip rectangle.
	Clip,

	/// @brief Keep lines that intersect the clip rectangle, including partial intersections.
	ClipPartial,
};
PTGN_SERIALIZE_ENUM(TextClipMode)

struct TextClipConstraint {
	Rect rect;
	TextClipMode mode{ TextClipMode::Clip };
};

struct DrawTextRequest {
	const TextLayout& layout;
	Color tint{ color::White };
	Depth depth;
	int entity_id{ -1 };
	std::span<const TextClipConstraint> clips;
	std::size_t reveal_glyph_count{ std::numeric_limits<std::size_t>::max() };
	float time{ 0.0f };
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

struct PreparedTextDraw {
	Transform transform;

	std::array<TextClipConstraint, 2> clips;
	std::size_t clip_count{ 0 };

	bool drawable{ true };

	std::span<const TextClipConstraint> GetClips() const {
		return { clips.data(), clip_count };
	}
};

[[nodiscard]] TextLayout BuildTextLayout(const ResolvedStyledText& styled_text, const TextBox& box);

[[nodiscard]] TextMeasurement MeasureText(
	const ResolvedStyledText& styled_text, const TextBox& box
);

std::vector<UniformWrite> GetTextUniforms(const DistanceFieldStyle& sdf, bool is_decoration);

std::vector<TextDrawBatch> BuildTextDrawBatches(const DrawTextRequest& request);

[[nodiscard]] bool TextLayoutFitsInBox(const TextLayout& layout, Rect box);

[[nodiscard]] PreparedTextDraw PrepareTextDraw(
	Transform transform, const TextLayout& layout, const TextBox& box, Origin origin,
	std::optional<TextClipConstraint> explicit_clip = std::nullopt
);

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
			style.require_three_letter_remainder, style.tab_width, style.max_lines,
			style.shrink_scale
		);
	}
};

template <>
struct std::hash<ptgn::TextBox> {
	std::size_t operator()(const ptgn::TextBox& box) const {
		return ptgn::Hash(box.rect, box.style);
	}
};
