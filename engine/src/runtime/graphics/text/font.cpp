#include "runtime/graphics/text/font.h"

#include <ecs/ecs.h>

#include <cstdint>
#include <optional>

#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "renderer/resources/id.h"
#include "renderer/text/font_atlas.h"

namespace ptgn {

std::optional<impl::GlyphMetrics> Font::GetGlyph(std::uint32_t codepoint) const {
	return GetEntity().Get<impl::FontAtlas>().GetGlyph(codepoint);
}

float Font::GetAdvance(std::uint32_t current_codepoint, std::uint32_t next_codepoint) const {
	return GetEntity().Get<impl::FontAtlas>().GetAdvance(current_codepoint, next_codepoint);
}

const impl::FontData& Font::GetFontData() const {
	return GetEntity().Get<impl::FontAtlas>().GetFontData();
}

impl::TextureId Font::GetAtlasTexture() const {
	return GetEntity().Get<impl::FontAtlas>().GetAtlasTexture();
}

V2_int Font::GetAtlasSize() const {
	return GetEntity().Get<impl::FontAtlas>().GetAtlasSize();
}

} // namespace ptgn
