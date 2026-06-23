#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/tolerance.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "renderer/text/font_style.h"
#include "renderer/text/text_glyph.h"
#include "serialization/serialize.h"

namespace ptgn {

namespace impl {

class FontAtlas;

} // namespace impl

inline constexpr float kDefaultBoldWeight{ 0.04f };

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

	constexpr bool operator==(const DistanceFieldStyle& o) const {
		return NearlyEqual(weight, o.weight) && NearlyEqual(softness, o.softness) &&
			   outline_color == o.outline_color && NearlyEqual(outline_width, o.outline_width) &&
			   NearlyEqual(outline_softness, o.outline_softness) &&
			   shadow_color == o.shadow_color && shadow_offset == o.shadow_offset &&
			   NearlyEqual(shadow_width, o.shadow_width) &&
			   NearlyEqual(shadow_softness, o.shadow_softness) &&
			   outer_glow_color == o.outer_glow_color &&
			   NearlyEqual(outer_glow_width, o.outer_glow_width) &&
			   NearlyEqual(outer_glow_softness, o.outer_glow_softness) &&
			   inner_glow_color == o.inner_glow_color &&
			   NearlyEqual(inner_glow_width, o.inner_glow_width) &&
			   NearlyEqual(inner_glow_softness, o.inner_glow_softness) &&
			   NearlyEqual(pixel_range, o.pixel_range);
	}

	PTGN_SERIALIZE(
		DistanceFieldStyle, weight, softness, outline_color, outline_width, outline_softness,
		shadow_color, shadow_offset, shadow_width, shadow_softness, outer_glow_color,
		outer_glow_width, outer_glow_softness, inner_glow_color, inner_glow_width,
		inner_glow_softness, pixel_range
	)
};

struct TextRunStyle {
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

	constexpr bool operator==(const TextRunStyle& o) const {
		return color == o.color && fake_bold_if_missing == o.fake_bold_if_missing &&
			   NearlyEqual(fake_bold_weight, o.fake_bold_weight) && NearlyEqual(scale, o.scale) &&
			   NearlyEqual(kerning, o.kerning) && NearlyEqual(tracking, o.tracking) &&
			   NearlyEqual(line_spacing, o.line_spacing) && flags == o.flags && sdf == o.sdf &&
			   effect.type == o.effect.type && NearlyEqual(effect.amplitude, o.effect.amplitude) &&
			   NearlyEqual(effect.frequency, o.effect.frequency) &&
			   NearlyEqual(effect.speed, o.effect.speed) &&
			   NearlyEqual(effect.phase, o.effect.phase);
	}

	PTGN_SERIALIZE(
		TextRunStyle, color, fake_bold_if_missing, fake_bold_weight, scale, kerning, tracking,
		line_spacing, flags, sdf, effect
	)
};

struct TextRun {
	std::string text;
	std::string font;
	TextRunStyle style;

	constexpr bool operator==(const TextRun&) const = default;

	PTGN_SERIALIZE(TextRun, text, font, style)
};

struct StyledText {
	std::vector<TextRun> runs;

	constexpr bool operator==(const StyledText&) const = default;

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

template <>
struct std::hash<ptgn::DistanceFieldStyle> {
	std::size_t operator()(const ptgn::DistanceFieldStyle& style) const {
		return ptgn::Hash(
			ptgn::QuantizeUnsigned(style.weight), ptgn::QuantizeUnsigned(style.softness),

			style.outline_color, ptgn::QuantizeUnsigned(style.outline_width),
			ptgn::QuantizeUnsigned(style.outline_softness),

			style.shadow_color, style.shadow_offset, ptgn::QuantizeUnsigned(style.shadow_width),
			ptgn::QuantizeUnsigned(style.shadow_softness),

			style.outer_glow_color, ptgn::QuantizeUnsigned(style.outer_glow_width),
			ptgn::QuantizeUnsigned(style.outer_glow_softness),

			style.inner_glow_color, ptgn::QuantizeUnsigned(style.inner_glow_width),
			ptgn::QuantizeUnsigned(style.inner_glow_softness),

			ptgn::QuantizeUnsigned(style.pixel_range)
		);
	}
};

template <>
struct std::hash<ptgn::TextRunStyle> {
	std::size_t operator()(const ptgn::TextRunStyle& style) const {
		return ptgn::Hash(
			style.color,

			style.fake_bold_if_missing, ptgn::QuantizeUnsigned(style.fake_bold_weight),

			ptgn::QuantizeUnsigned(style.scale), ptgn::QuantizeSigned(style.kerning),
			ptgn::QuantizeSigned(style.tracking), ptgn::QuantizeSigned(style.line_spacing),

			std::to_underlying(style.flags),

			style.sdf,

			std::to_underlying(style.effect.type), ptgn::QuantizeSigned(style.effect.amplitude),
			ptgn::QuantizeSigned(style.effect.frequency), ptgn::QuantizeSigned(style.effect.speed),
			ptgn::QuantizeSigned(style.effect.phase)
		);
	}
};

template <>
struct std::hash<ptgn::TextRun> {
	std::size_t operator()(const ptgn::TextRun& run) const {
		return ptgn::Hash(run.text, run.font, run.style);
	}
};

template <>
struct std::hash<ptgn::StyledText> {
	std::size_t operator()(const ptgn::StyledText& styled_text) const {
		return ptgn::Hash(styled_text.runs);
	}
};