#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/texture.h"

namespace ptgn {

class Renderer;

namespace impl {

class FontData {
public:
	virtual ~FontData() = default;

	virtual std::optional<GlyphMetrics> GetGlyph(std::uint32_t codepoint) const = 0;
	virtual float GetAdvance(std::uint32_t current_codepoint, std::uint32_t next_codepoint)
		const										   = 0;
	virtual FontMetrics GetFontMetrics() const		   = 0;
	virtual std::uint64_t GetFontId() const			   = 0;
	virtual std::uint32_t GetAtlasTextureIndex() const = 0;
	virtual TextureId GetAtlasTexture() const		   = 0;
};

class MsdfFontData final : public FontData {
public:
	struct CreateInfo {
		float em_size{ 48.0f };
		float pixel_range{ 4.0f };
		double max_corner_angle{ 3.0 };
		std::uint32_t charset_begin{ 0x20 };
		std::uint32_t charset_end{ 0xFF };
		std::uint32_t thread_count{ 8 };
	};

	MsdfFontData() = default;

	MsdfFontData(
		Renderer& renderer, const path& font_path, std::uint32_t atlas_texture_index,
		CreateInfo create_info = {}
	);

	MsdfFontData(
		std::uint64_t font_id, TextureObject&& atlas_texture, std::uint32_t atlas_texture_index,
		V2_int atlas_size, FontMetrics metrics
	);

	std::optional<GlyphMetrics> GetGlyph(std::uint32_t codepoint) const override;
	float GetAdvance(std::uint32_t current_codepoint, std::uint32_t next_codepoint) const override;
	FontMetrics GetFontMetrics() const override;
	std::uint64_t GetFontId() const override;
	std::uint32_t GetAtlasTextureIndex() const override;
	TextureId GetAtlasTexture() const override;

	V2_int GetAtlasSize() const;

private:
	static std::uint64_t KerningKey(std::uint32_t current_codepoint, std::uint32_t next_codepoint);

	TextureObject atlas_texture_;
	FontMetrics metrics_;

	std::unordered_map<std::uint32_t, GlyphMetrics> glyphs_;
	std::unordered_map<std::uint64_t, float> kerning_;
};

} // namespace impl

} // namespace ptgn