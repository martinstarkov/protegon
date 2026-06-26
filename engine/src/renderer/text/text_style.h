#pragma once

#include <algorithm>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/tolerance.h"
#include "core/math/vector2.h"
#include "renderer/text/font_style.h"
#include "renderer/text/text_glyph.h"
#include "serialization/serialize.h"

namespace ptgn {

/// @brief Default engine font key.
/// Do not modify this value.
/// Use ctx().font.SetDefault(new_default_font_key); to change the default font
inline constexpr std::string_view kDefaultFont{ "" };

/// @brief Default font size used when no explicit size is specified for text rendering.
inline constexpr float kDefaultFontSize{ 18.0f };

namespace impl {

class FontAtlas;

} // namespace impl

inline constexpr float kDefaultBoldWeight{ 0.04f };

struct DistanceFieldLayerStyle {
	Color color{ color::Transparent };
	float width{ 0.0f };
	float softness{ 1.0f };

	constexpr bool operator==(const DistanceFieldLayerStyle& o) const {
		return color == o.color && NearlyEqual(width, o.width) && NearlyEqual(softness, o.softness);
	}

	PTGN_SERIALIZE(DistanceFieldLayerStyle, color, width, softness)
};

struct DistanceFieldStyle {
	float weight{ 0.5f };
	float softness{ 1.0f };

	DistanceFieldLayerStyle outline;

	DistanceFieldLayerStyle shadow;
	V2_float shadow_offset;

	DistanceFieldLayerStyle outer_glow;
	DistanceFieldLayerStyle inner_glow;

	float pixel_range{ 0.0f };

	constexpr bool operator==(const DistanceFieldStyle& o) const {
		return NearlyEqual(weight, o.weight) && NearlyEqual(softness, o.softness) &&
			   outline == o.outline && shadow == o.shadow && shadow_offset == o.shadow_offset &&
			   outer_glow == o.outer_glow && inner_glow == o.inner_glow &&
			   NearlyEqual(pixel_range, o.pixel_range);
	}

	PTGN_SERIALIZE(
		DistanceFieldStyle, weight, softness, outline, shadow, shadow_offset, outer_glow,
		inner_glow, pixel_range
	)
};

struct TextRunStyle {
	Color color{ color::White };

	float bold_weight{ kDefaultBoldWeight };

	float size{ kDefaultFontSize };
	float line_spacing{ 0.0f };

	/// Multiplier applied to the font's pair-specific kerning.
	///
	/// 1.0 uses the font's normal kerning.
	/// 0.0 disables kerning.
	float kerning{ 1.0f };

	/// Additional spacing between adjacent glyphs, in em units.
	///
	/// 0.05 adds 5% of the rendered font size between glyphs.
	/// Negative values bring glyphs closer together.
	float tracking{ 0.0f };

	FontStyle flags{ FontStyle::Normal };

	DistanceFieldStyle sdf;
	GlyphEffectStyle effect;

	constexpr bool operator==(const TextRunStyle& o) const {
		return color == o.color && NearlyEqual(bold_weight, o.bold_weight) &&
			   NearlyEqual(size, o.size) && NearlyEqual(kerning, o.kerning) &&
			   NearlyEqual(tracking, o.tracking) && NearlyEqual(line_spacing, o.line_spacing) &&
			   flags == o.flags && sdf == o.sdf && effect.type == o.effect.type &&
			   NearlyEqual(effect.amplitude, o.effect.amplitude) &&
			   NearlyEqual(effect.frequency, o.effect.frequency) &&
			   NearlyEqual(effect.speed, o.effect.speed) &&
			   NearlyEqual(effect.phase, o.effect.phase);
	}

	PTGN_SERIALIZE(
		TextRunStyle, color, bold_weight, size, kerning, tracking, line_spacing, flags, sdf, effect
	)
};

struct TextRun {
	std::string text;
	std::string font{ kDefaultFont };
	TextRunStyle style;

	constexpr bool operator==(const TextRun&) const = default;

	PTGN_SERIALIZE(TextRun, text, font, style)
};

struct StyledText {
	std::vector<TextRun> runs;

	constexpr StyledText() = default;

	constexpr StyledText(std::initializer_list<TextRun> text_runs) : runs{ text_runs } {}

	constexpr bool operator==(const StyledText&) const = default;

	constexpr bool HasContent() const {
		return std::ranges::any_of(runs, [](const auto& run) { return !run.text.empty(); });
	}

	PTGN_SERIALIZE_VALUE(StyledText, runs)
};

namespace impl {

struct ResolvedTextRun {
	std::string text;
	const FontAtlas* font{ nullptr };
	TextRunStyle style;

	constexpr bool operator==(const ResolvedTextRun&) const = default;
};

struct ResolvedStyledText {
	std::vector<ResolvedTextRun> runs;

	constexpr bool operator==(const ResolvedStyledText&) const = default;
};

} // namespace impl

} // namespace ptgn