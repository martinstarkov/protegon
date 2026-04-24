#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "runtime/graphics/text/text_effect.h"

namespace ptgn {

namespace impl {

class FontData;

} // namespace impl

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
	Clip,
	Ellipsis,
	ShrinkToFit,
};

enum class FontStyleFlags : std::uint32_t {
	None		  = 0,
	Bold		  = 1 << 0,
	Italic		  = 1 << 1,
	Underline	  = 1 << 2,
	Strikethrough = 1 << 3,
};

inline FontStyleFlags operator|(FontStyleFlags a, FontStyleFlags b) {
	return static_cast<FontStyleFlags>(std::to_underlying(a) | std::to_underlying(b));
}

inline bool HasFlag(FontStyleFlags value, FontStyleFlags flag) {
	return (std::to_underlying(value) & std::to_underlying(flag)) != 0u;
}

struct DistanceFieldStyle {
	float weight{ 0.5f };
	float softness{ 0.1f };

	Color outline_color{ color::Black.WithAlpha(0) };
	float outline_width{ 0.0f };
	float outline_softness{ 0.1f };

	Color shadow_color{ color::Black.WithAlpha(0) };
	V2_float shadow_offset;
	float shadow_softness{ 0.2f };

	Color glow_color{ color::White.WithAlpha(0) };
	float glow_outer_width{ 0.0f };
	float glow_softness{ 0.3f };

	bool operator==(const DistanceFieldStyle&) const = default;
};

struct TextRunStyle {
	impl::FontData* font{ nullptr };
	impl::FontData* bold_font{ nullptr };
	impl::FontData* italic_font{ nullptr };
	impl::FontData* bold_italic_font{ nullptr };
	Color color{ color::White };

	bool fake_bold_if_missing{ true };
	float fake_bold_weight{ 0.08f };

	float scale{ 1.0f };
	float kerning{ 0.0f };
	float tracking{ 0.0f };
	float line_spacing{ 0.0f };

	FontStyleFlags flags{ FontStyleFlags::None };

	DistanceFieldStyle sdf;
	GlyphEffectStyle effect;

	[[nodiscard]] bool IsUsingFakeBold() const;

	impl::FontData* GetFont() const;
};

struct TextRun {
	std::string text;
	TextRunStyle style;
};

struct StyledText {
	std::vector<TextRun> runs;
};

struct TextStyle {
	impl::FontData* font{ nullptr };
	Color color{ color::White };
	float scale{ 1.0f };
	FontStyleFlags flags{ FontStyleFlags::None };
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