#pragma once

#include <cstdint>
#include <optional>

#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "core/util/hash.h"
#include "renderer/resources/id.h"
#include "renderer/text/font_atlas.h"

namespace ptgn {

class Font : public EntityHandle {
public:
	using EntityHandle::EntityHandle;

	std::optional<impl::GlyphMetrics> GetGlyph(std::uint32_t codepoint) const;

	float GetAdvance(std::uint32_t current_codepoint, std::uint32_t next_codepoint) const;

	const impl::FontData& GetFontData() const;

	impl::TextureId GetAtlasTexture() const;

	[[nodiscard]] V2_int GetAtlasSize() const;
};

} // namespace ptgn

template <>
struct std::hash<ptgn::Font> {
	std::size_t operator()(const ptgn::Font& font) const {
		return ptgn::Hash(font.GetEntity());
	}
};