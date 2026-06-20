#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"

namespace ptgn {

class Renderer;

inline constexpr float kDefaultFontSize{ 18.0f };

struct FontBinary {
	constexpr FontBinary() = default;

	constexpr FontBinary(const std::uint8_t* font_buffer, std::size_t buffer_length) :
		buffer{ font_buffer }, length{ buffer_length } {}

	const std::uint8_t* buffer{ nullptr };
	std::size_t length{ 0 };
};

namespace impl {

struct FontAtlasInfo {
	constexpr FontAtlasInfo() = default;

	float em_size{ 40.0f };
	float em_range{ 0.2f };
	// float pixel_range{ 2.0f };
	float max_corner_angle{ 3.0f };
	float miter_limit{ 1.0f };
	int thread_count{ 8 };
	std::uint32_t charset_begin{ 0x20 };
	std::uint32_t charset_end{ 0xFF };
};

struct GlyphMetrics {
	std::uint32_t codepoint{ 0 };
	float advance{ 0.0f };
	Rect plane;
	Rect uv;
};

struct FontMetrics {
	float ascender{ 0.0f };
	float descender{ 0.0f };
	float line_height{ 0.0f };
	float em_size{ 0.0f };
	float pixel_range{ 0.0f };
};

struct FontData {
	path font_path;
	FontMetrics metrics;
	/// @brief Key is a Unicode codepoint. Value is the glyph metrics for that codepoint.
	std::unordered_map<std::uint32_t, GlyphMetrics> glyphs;
	/// @brief Key is a 64-bit integer where the high 32 bits are the current codepoint and the low
	/// 32 bits are the next codepoint. Value is the kerning adjustment to apply to the advance when
	/// the current codepoint is followed by the next codepoint.
	std::unordered_map<std::uint64_t, float> kerning;
};

class FontAtlas {
public:
	FontAtlas() = default;

	/// @brief Generate font atlas and embed FontData into cached PNG file.
	FontAtlas(
		Renderer& renderer, path font_path, path cache_png_path,
		const FontAtlasInfo& atlas_info = {}
	);

	/// @brief Load cached PNG from disk instead of generating a new one.
	FontAtlas(Renderer& renderer, path cache_png_path);

	/// @brief Load built-in font from generated header binary.
	FontAtlas(Renderer& renderer, FontBinary font_png);

	std::optional<GlyphMetrics> GetGlyph(std::uint32_t codepoint) const;

	float GetAdvance(std::uint32_t current_codepoint, std::uint32_t next_codepoint) const;

	const FontData& GetFontData() const;

	TextureId GetAtlasTexture() const;

	V2_int GetAtlasSize() const;

private:
	TextureObject atlas_texture_;

	FontData data_;
};

bool IsFontAtlasPng(const path& png_path);

} // namespace impl

} // namespace ptgn