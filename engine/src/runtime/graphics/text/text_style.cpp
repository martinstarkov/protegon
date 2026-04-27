#include "runtime/graphics/text/text_style.h"

#include <ostream>
#include <utility>

#include "core/graphics/color.h"
#include "core/log.h"
#include "core/util/hash.h"
#include "runtime/graphics/text/font_data.h"
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
		if ((style | flag) == flag) {
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

std::size_t std::hash<ptgn::DistanceFieldStyle>::operator()(const ptgn::DistanceFieldStyle& style
) const {
	return ptgn::Hash(
		style.weight, style.softness, style.outline_color, style.outline_width,
		style.outline_softness, style.glow_color, style.glow_outer_width, style.glow_softness
	);
}

std::size_t std::hash<ptgn::TextRunStyle>::operator()(const ptgn::TextRunStyle& style) const {
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