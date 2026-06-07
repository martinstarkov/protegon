#include "runtime/graphics/text/text_style.h"

#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "core/graphics/color.h"
#include "core/log.h"
#include "core/util/hash.h"
#include "runtime/graphics/text/text_effect.h"

namespace ptgn {

TextRunStyle TextStyle::ToRunStyle() const {
	TextRunStyle run;
	run.font  = font;
	run.color = color;
	run.scale = scale;
	run.flags = flags;
	run.sdf	  = sdf;
	return run;
}

std::ostream& operator<<(std::ostream& os, FontStyle style) {
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

} // namespace ptgn

std::size_t std::hash<ptgn::DistanceFieldStyle>::operator()(
	const ptgn::DistanceFieldStyle& style
) const {
	// TODO: Quantize floats.
	return ptgn::Hash(
		style.weight, style.softness,

		style.outline_color, style.outline_width, style.outline_softness,

		style.shadow_color, style.shadow_offset, style.shadow_width, style.shadow_softness,

		style.outer_glow_color, style.outer_glow_width, style.outer_glow_softness,

		style.inner_glow_color, style.inner_glow_width, style.inner_glow_softness,

		style.pixel_range
	);
}

std::size_t std::hash<ptgn::TextRunStyle>::operator()(const ptgn::TextRunStyle& style) const {
	// TODO: Quantize floats.
	return ptgn::Hash(
		style.font, style.color, style.scale, style.kerning, style.tracking, style.line_spacing,
		style.fake_bold_if_missing, style.fake_bold_weight, std::to_underlying(style.flags),
		style.sdf, std::to_underlying(style.effect.type), style.effect.amplitude,
		style.effect.frequency, style.effect.speed, style.effect.phase
	);
}

std::size_t std::hash<ptgn::TextRun>::operator()(const ptgn::TextRun& run) const {
	return ptgn::Hash(run.text, run.style);
}

std::size_t std::hash<ptgn::StyledText>::operator()(const ptgn::StyledText& styled_text) const {
	return ptgn::Hash(styled_text.runs);
}