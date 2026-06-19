#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "renderer/text/font_style.h"
#include "renderer/text/glyph.h"

namespace ptgn {

inline constexpr float kDefaultBoldWeight{ 0.04f };

enum class HorizontalAlign : std::uint8_t {
	Left,
	Center,
	Right,
	Justify,
};

enum class VerticalAlign : std::uint8_t {
	Top,
	Center,
	Bottom,
};

enum class WrapMode : std::uint8_t {
	None,
	Word,
	Character,
};

enum class OverflowMode : std::uint8_t {
	Overflow,
	/// @brief Hide any glyph not fully inside the rect.
	Clip,
	/// @brief Hide glyphs only when fully outside the rect.
	ClipPartial,
	Ellipsis,
	ScaleToFit,
};

namespace impl {

struct DistanceFieldStyle {
	float weight{ 0.5f };
	float softness{ 1.0f };

	Color outline_color{ color::Black.WithAlpha(0) };
	float outline_width{ 0.0f };
	float outline_softness{ 1.0f };

	Color shadow_color{ color::Black.WithAlpha(0) };
	V2_float shadow_offset;
	float shadow_width{ 0.0f };
	float shadow_softness{ 1.0f };

	Color outer_glow_color{ color::White.WithAlpha(0) };
	float outer_glow_width{ 0.0f };
	float outer_glow_softness{ 1.0f };

	Color inner_glow_color{ color::White.WithAlpha(0) };
	float inner_glow_width{ 0.0f };
	float inner_glow_softness{ 1.0f };

	float pixel_range{ 0.0f };

	bool operator==(const DistanceFieldStyle&) const = default;
};

struct TextRunStyle {
	std::string font;
	Color color{ color::White };

	bool fake_bold_if_missing{ true };
	float fake_bold_weight{ kDefaultBoldWeight };

	float scale{ 1.0f };
	float kerning{ 0.0f };
	float tracking{ 0.0f };
	float line_spacing{ 0.0f };

	FontStyle flags{ FontStyle::Normal };

	DistanceFieldStyle sdf;
	GlyphEffectStyle effect;
};

struct TextRun {
	std::string text;
	TextRunStyle style;
};

struct StyledText {
	std::vector<TextRun> runs;
};

} // namespace impl

} // namespace ptgn

template <>
struct std::hash<ptgn::impl::DistanceFieldStyle> {
	std::size_t operator()(const ptgn::impl::DistanceFieldStyle& style) const {
		// TODO: Quantize floats by converting to integers.
		return ptgn::Hash(
			style.weight, style.softness,

			style.outline_color, style.outline_width, style.outline_softness,

			style.shadow_color, style.shadow_offset, style.shadow_width, style.shadow_softness,

			style.outer_glow_color, style.outer_glow_width, style.outer_glow_softness,

			style.inner_glow_color, style.inner_glow_width, style.inner_glow_softness,

			style.pixel_range
		);
	}
};

template <>
struct std::hash<ptgn::impl::TextRunStyle> {
	std::size_t operator()(const ptgn::impl::TextRunStyle& style) const {
		// TODO: Quantize floats by converting to integers.
		return ptgn::Hash(
			style.font, style.color, style.scale, style.kerning, style.tracking, style.line_spacing,
			style.fake_bold_if_missing, style.fake_bold_weight, std::to_underlying(style.flags),
			style.sdf, std::to_underlying(style.effect.type), style.effect.amplitude,
			style.effect.frequency, style.effect.speed, style.effect.phase
		);
	}
};

template <>
struct std::hash<ptgn::impl::TextRun> {
	std::size_t operator()(const ptgn::impl::TextRun& run) const {
		return ptgn::Hash(run.text, run.style);
	}
};

template <>
struct std::hash<ptgn::impl::StyledText> {
	std::size_t operator()(const ptgn::impl::StyledText& styled_text) const {
		return ptgn::Hash(styled_text.runs);
	}
};