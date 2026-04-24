#include "runtime/graphics/text/text_layout.h"

#include <string_view>

#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
#include "runtime/graphics/text/font_data.h"
#include "runtime/graphics/text/text_style.h"
#include "runtime/graphics/text/text_system.h"

namespace ptgn {

TextLayoutRequest MakeTextRequest(
	std::string_view content, impl::FontData& font, Rect rect, float scale, Color color,
	const TextLayoutStyle& layout_style
) {
	TextRunStyle run;
	run.font  = &font;
	run.scale = scale;
	run.color = color;

	TextLayoutRequest request;
	request.styled_text = TextSystem::MakePlainText(content, run);
	request.box.rect	= rect;
	request.box.style	= layout_style;
	return request;
}

} // namespace ptgn
