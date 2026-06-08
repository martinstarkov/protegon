#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "runtime/graphics/text/font_style.h"
#include "runtime/graphics/text/text_effect.h"

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

struct TextStyle {
	std::string font;
	Color color{ color::White };
	float scale{ 1.0f };
	FontStyle flags{ FontStyle::Normal };
	DistanceFieldStyle sdf;

	[[nodiscard]] TextRunStyle ToRunStyle() const;
};

} // namespace ptgn

template <>
struct std::hash<ptgn::DistanceFieldStyle> {
	std::size_t operator()(const ptgn::DistanceFieldStyle& style) const;
};

template <>
struct std::hash<ptgn::TextRunStyle> {
	std::size_t operator()(const ptgn::TextRunStyle& style) const;
};

template <>
struct std::hash<ptgn::TextRun> {
	std::size_t operator()(const ptgn::TextRun& run) const;
};

template <>
struct std::hash<ptgn::StyledText> {
	std::size_t operator()(const ptgn::StyledText& styled_text) const;
};