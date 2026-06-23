#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "core/graphics/surface.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

class Renderer;

/// @brief Default font size used when no explicit size is specified for text rendering.
inline constexpr float kDefaultFontSize{ 18.0f };

/// @brief Configuration for font atlas generation. These values affect the quality and size of the
/// generated atlas texture and the resulting glyph metrics.
struct FontAtlasInfo {
	constexpr FontAtlasInfo() = default;

	/// @brief Determines how large glyph geometry is represented during atlas generation. A larger
	/// value generally provides more source resolution but also tends to require more atlas space
	float em_size{ 40.0f };

	/// @brief Determines how far outside and inside the glyph outline the signed-distance field
	/// records useful distance information.
	/// If the font looks bad, make sure 2 / scale < atlas.distanceRange < 128 (see issue 11 on
	/// msdf-atlas-gen), where atlas.distanceRange = (em_range[1] - em_range[0]) * atlas.size and
	/// scale is the ratio of the on-screen size to the size in the atlas.
	float em_range{ 0.2f };

	/// @brief Pixel range calculated from em_size * em_range. A larger range gives the shader more
	/// distance-field space for effects such as outlines and glows. However, a larger range may
	/// also require more padding around each glyph and therefore more reduced the resolution of the
	/// glyph itself.
	/// float pixel_range{ 2.0f };

	/// @brief This value acts as an angular threshold for deciding whether two neighboring edges
	/// form a corner that should be treated separately.
	float max_corner_angle{ 3.0f };

	/// @brief Controls how sharply glyph bounds or distance-field padding may extend around
	/// corners. The miter limit prevents those corners from producing excessively large bounds or
	/// padding. A larger value permits longer pointed extensions. A smaller value limits them more
	/// aggressively.
	float miter_limit{ 1.0f };

	/// @brief Number of worker threads requested for atlas generation.
	int thread_count{ 8 };

	/// @brief First Unicode codepoint to include when generating the atlas.
	std::uint32_t charset_begin{ 0x20 }; /// Default: SPACE

	/// @brief Last Unicode codepoint to include when generating the atlas.
	std::uint32_t charset_end{ 0xFF }; /// Default: LATIN SMALL LETTER Y WITH DIAERESIS
};

namespace impl {

constexpr TextureFormat kFontAtlasFormat{ TextureFormat::RGBA8 };
constexpr TextureParams kFontAtlasTextureParams{ TextureMinFilter::Linear,
												 TextureMagFilter::Linear };
constexpr int kFontAtlasChannelCount{ GetChannelCount(kFontAtlasFormat) };

/// @brief Measurements for a single glyph.
struct GlyphMetrics {
	/// @brief The Unicode codepoint represented by this glyph.
	std::uint32_t codepoint{ 0 };

	/// @brief Horizontal distance to move the text cursor after placing this glyph.
	float advance{ 0.0f };

	/// @brief The glyph's bounds in the font's layout coordinate system, sometimes called plane
	/// bounds. This rectangle describes where the glyph quad should be positioned relative to its
	/// pen position or baseline.
	Rect plane;

	/// @brief Normalized texture coordinates of the glyph within the atlas texture.
	Rect uv;
};

/// @brief Font-wide measurements shared by all glyphs.
struct FontMetrics {
	/// @brief Distance from the baseline to the font's furthest upper extent. Typically positive.
	float ascender{ 0.0f };

	/// @brief Distance from the baseline to the font's furthest lower extent. Typically negative.
	float descender{ 0.0f };

	/// @brief Recommended baseline-to-baseline distance between adjacent lines. Not necessarily
	/// equal to ascender - descender because the font may include additional line gap.
	float line_height{ 0.0f };

	/// @brief This provides the reference size used to scale font-space measurements to a requested
	/// text size.
	float em_size{ 0.0f };

	/// @brief Distance-field range represented in atlas texels. This tells the shader how encoded
	/// texture values correspond to actual signed distance.
	float pixel_range{ 0.0f };
};

struct FontData {
	/// @brief Path of the original font file used to generate this atlas.
	path path;

	FontMetrics metrics;

	/// @brief Key is a Unicode codepoint. Value is the glyph metrics for that codepoint.
	std::unordered_map<std::uint32_t, GlyphMetrics> glyphs;

	/// @brief Key is a 64-bit integer where the high 32 bits are the current codepoint and the low
	/// 32 bits are the next codepoint. Value is the kerning adjustment to apply to the advance when
	/// the current codepoint is followed by the next codepoint.
	/// Kerning adjustment may be negative to move the glyphs closer together or positive to move
	/// them further apart.
	/// For example:
	/// current = U'A'; next = U'V';
	/// advance = current.advance + kerning[current << 32 | next];
	std::unordered_map<std::uint64_t, float> kerning;
};

struct FontBinary {
	constexpr FontBinary() = default;

	constexpr FontBinary(const std::uint8_t* font_buffer, std::size_t buffer_length) :
		buffer{ font_buffer }, length{ buffer_length } {}

	const std::uint8_t* buffer{ nullptr };
	std::size_t length{ 0 };
};

/// @brief CPU-side atlas data, produced either by generation or cache loading.
struct FontAtlasData {
	Surface surface;
	FontData font;
};

class FontAtlas {
public:
	FontAtlas() = default;

	/// @brief Generate an atlas and write it to a cached PNG with embedded FontData.
	FontAtlas(
		Renderer& renderer, path font_path, const path& cache_png_path,
		const FontAtlasInfo& atlas_info = {}
	);

	/// @brief Load cached PNG with embedded FontData from disk instead of generating a new one.
	FontAtlas(Renderer& renderer, path cache_png_path);

	/// @brief Load built-in font from generated header binary.
	FontAtlas(Renderer& renderer, FontBinary font_png);

	/// @return std::nullopt if the requested codepoint is not present in the font atlas.
	std::optional<GlyphMetrics> GetGlyph(std::uint32_t codepoint) const;

	/// @return The amount by which the text cursor should move after the current glyph when it is
	/// followed by the next glyph. This includes the current glyph's advance plus any kerning
	/// adjustment for the specific pair of glyphs. Returns 0 if the current codepoint is not
	/// present in the font atlas.
	float GetAdvance(std::uint32_t current_codepoint, std::uint32_t next_codepoint) const;

	/// @return The font-wide metrics for this atlas.
	FontMetrics GetMetrics() const;

	TextureId GetTexture() const;

	/// @return The size of the atlas texture in pixels.
	V2_int GetSize() const;

private:
	void Initialize(Renderer& renderer, FontAtlasData&& payload);

	TextureObject atlas_texture_;

	FontData data_;
};

} // namespace impl

} // namespace ptgn