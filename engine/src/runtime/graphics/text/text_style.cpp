#include "runtime/graphics/text/text_style.h"

#include <utility>

#include "core/graphics/color.h"
#include "core/util/hash.h"
#include "runtime/graphics/text/font_data.h"
#include "runtime/graphics/text/text_effect.h"

namespace ptgn {

bool TextRunStyle::IsUsingFakeBold() const {
	const bool wants_bold	= HasFlag(flags, FontStyleFlags::Bold);
	const bool wants_italic = HasFlag(flags, FontStyleFlags::Italic);

	if (!wants_bold || !fake_bold_if_missing) {
		return false;
	}

	if (wants_italic) {
		return bold_italic_font == nullptr;
	}

	return bold_font == nullptr;
}

impl::FontData* TextRunStyle::GetFont() const {
	const bool bold	  = HasFlag(flags, FontStyleFlags::Bold);
	const bool italic = HasFlag(flags, FontStyleFlags::Italic);

	if (bold && italic && bold_italic_font != nullptr) {
		return bold_italic_font;
	}
	if (bold && bold_font != nullptr) {
		return bold_font;
	}
	if (italic && italic_font != nullptr) {
		return italic_font;
	}
	return font;
}

TextRunStyle TextStyle::ToRunStyle() const {
	TextRunStyle run;
	run.font  = font;
	run.color = color;
	run.scale = scale;
	run.flags = flags;
	run.sdf	  = sdf;
	return run;
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
		style.font != nullptr ? style.font->GetFontId() : 0ULL,
		style.bold_font != nullptr ? style.bold_font->GetFontId() : 0ULL,
		style.italic_font != nullptr ? style.italic_font->GetFontId() : 0ULL,
		style.bold_italic_font != nullptr ? style.bold_italic_font->GetFontId() : 0ULL, style.color,
		style.scale, style.kerning, style.tracking, style.line_spacing, style.fake_bold_if_missing,
		style.fake_bold_weight, std::to_underlying(style.flags), style.sdf,
		std::to_underlying(style.effect.type), style.effect.amplitude, style.effect.frequency,
		style.effect.speed, style.effect.phase
	);
}

std::size_t std::hash<ptgn::TextRun>::operator()(const ptgn::TextRun& run) const {
	return ptgn::Hash(run.text, run.style);
}

std::size_t std::hash<ptgn::StyledText>::operator()(const ptgn::StyledText& styled_text) const {
	return ptgn::Hash(styled_text.runs);
}