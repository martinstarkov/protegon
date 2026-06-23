#include "renderer/text/font_atlas.h"

#include <msdf-atlas-gen/AtlasGenerator.h>
#include <msdf-atlas-gen/BitmapAtlasStorage.h>
#include <msdf-atlas-gen/Charset.h>
#include <msdf-atlas-gen/FontGeometry.h>
#include <msdf-atlas-gen/glyph-generators.h>
#include <msdf-atlas-gen/GlyphGeometry.h>
#include <msdf-atlas-gen/TightAtlasPacker.h>
#include <msdf-atlas-gen/types.h>
#include <msdfgen/core/edge-coloring.h>
#include <msdfgen/core/generator-config.h>
#include <msdfgen/ext/import-font.h>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <list>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <msdf-atlas-gen/ImmediateAtlasGenerator.hpp>
#include <msdfgen/core/BitmapRef.hpp>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/surface.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/text/font_cache.h"

namespace ptgn::impl {

namespace {

using FontAtlasDataType = std::uint8_t;

constexpr float kFontScale{ 1.0f };

using AtlasGenerator = msdf_atlas::ImmediateAtlasGenerator<
	float, kFontAtlasChannelCount, &msdf_atlas::mtsdfGenerator,
	msdf_atlas::BitmapAtlasStorage<FontAtlasDataType, kFontAtlasChannelCount> >;

auto InitFreetype() {
	auto freetype{ msdfgen::initializeFreetype() };
	PTGN_ASSERT(freetype, "Failed to initialize FreeType for MSDF font generation");
	return std::unique_ptr<msdfgen::FreetypeHandle, void (*)(msdfgen::FreetypeHandle*)>{
		freetype,
		[](msdfgen::FreetypeHandle* h) {
			if (h) {
				msdfgen::deinitializeFreetype(h);
			}
		}
	};
}

auto LoadFont(auto& freetype, const path& font_path) {
	auto font{ msdfgen::loadFont(freetype.get(), font_path.string().c_str()) };
	PTGN_ASSERT(font, "Failed to load font: ", font_path.string());
	return std::unique_ptr<msdfgen::FontHandle, void (*)(msdfgen::FontHandle*)>{
		font,
		[](msdfgen::FontHandle* f) {
			if (f) {
				msdfgen::destroyFont(f);
			}
		}
	};
}

std::uint64_t ToKerningKey(std::uint32_t current_codepoint, std::uint32_t next_codepoint) {
	return (static_cast<std::uint64_t>(current_codepoint) << 32ULL) |
		   static_cast<std::uint64_t>(next_codepoint);
}

FontAtlasData GenerateFontAtlas(path font_path, const FontAtlasInfo& atlas_info) {
	auto freetype{ InitFreetype() };

	font_path = GetAbsolutePath(font_path);

	auto font_face{ LoadFont(freetype, font_path) };

	std::vector<msdf_atlas::GlyphGeometry> glyphs;
	msdf_atlas::FontGeometry font_geometry{ &glyphs };

	msdf_atlas::Charset charset;
	for (std::uint32_t c{ atlas_info.charset_begin }; c <= atlas_info.charset_end; ++c) {
		charset.add(c);
	}

	auto loaded_glyph_count{ font_geometry.loadCharset(font_face.get(), kFontScale, charset) };
	PTGN_ASSERT(
		loaded_glyph_count > 0, "Failed to load ", loaded_glyph_count,
		" glyphs for font: ", font_path.string()
	);

	std::uint64_t glyph_seed{ 0 };
	for (auto& glyph : glyphs) {
		glyph_seed = glyph_seed * 6364136223846793005ULL + 1442695040888963407ULL;
		glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, atlas_info.max_corner_angle, glyph_seed);
	}

	msdf_atlas::TightAtlasPacker packer;
	packer.setDimensionsConstraint(msdf_atlas::DimensionsConstraint::SQUARE);
	packer.setScale(atlas_info.em_size);
	packer.setUnitRange(atlas_info.em_range); // or packer.setPixelRange(atlas_info.pixel_range);
	packer.setMiterLimit(atlas_info.miter_limit);

	auto remaining_glyph_count{ packer.pack(glyphs.data(), static_cast<int>(glyphs.size())) };
	PTGN_ASSERT(
		remaining_glyph_count == 0, "Failed to pack ", remaining_glyph_count,
		" MSDF glyphs into atlas for font: ", font_path.string()
	);

	V2_int atlas_size;
	packer.getDimensions(atlas_size.x, atlas_size.y);

	msdf_atlas::GeneratorAttributes attributes;
	attributes.config.overlapSupport = true;
	attributes.scanlinePass			 = true;

	AtlasGenerator generator{ atlas_size.x, atlas_size.y };
	generator.setAttributes(attributes);
	generator.setThreadCount(atlas_info.thread_count);
	generator.generate(glyphs.data(), static_cast<int>(glyphs.size()));

	msdfgen::BitmapConstRef<FontAtlasDataType, kFontAtlasChannelCount> bitmap{
		generator.atlasStorage()
	};

	atlas_size = { bitmap.width, bitmap.height };

	auto byte_count{ static_cast<std::size_t>(bitmap.width) * bitmap.height *
					 kFontAtlasChannelCount };

	Surface surface{ atlas_size, std::span<const FontAtlasDataType>{ bitmap.pixels, byte_count },
					 kFontAtlasChannelCount, true };

	FontData font;
	font.path = std::move(font_path);

	const auto& msdf_metrics{ font_geometry.getMetrics() };
	font.metrics.ascender	 = static_cast<float>(msdf_metrics.ascenderY);
	font.metrics.descender	 = static_cast<float>(msdf_metrics.descenderY);
	font.metrics.line_height = static_cast<float>(msdf_metrics.lineHeight);
	font.metrics.em_size	 = static_cast<float>(packer.getScale());
	font.metrics.pixel_range =
		atlas_info.em_range * font.metrics.em_size; // or atlas_info.pixel_range
	PTGN_ASSERT(
		font.metrics.em_size == atlas_info.em_size,
		"Failed to create font with em size: ", atlas_info.em_size
	);

	font.glyphs.reserve(glyphs.size());

	for (const auto& glyph : glyphs) {
		double plane_left{ 0.0 };
		double plane_bottom{ 0.0 };
		double plane_right{ 0.0 };
		double plane_top{ 0.0 };

		double atlas_left{ 0.0 };
		double atlas_bottom{ 0.0 };
		double atlas_right{ 0.0 };
		double atlas_top{ 0.0 };

		glyph.getQuadPlaneBounds(plane_left, plane_bottom, plane_right, plane_top);

		glyph.getQuadAtlasBounds(atlas_left, atlas_bottom, atlas_right, atlas_top);

		GlyphMetrics output;
		output.codepoint = glyph.getCodepoint();
		output.advance	 = static_cast<float>(glyph.getAdvance());

		output.plane = Rect{ { plane_left, -plane_top }, { plane_right, -plane_bottom } };

		output.uv = Rect{ { atlas_left / bitmap.width, 1.0f - atlas_top / bitmap.height },
						  { atlas_right / bitmap.width, 1.0f - atlas_bottom / bitmap.height } };

		font.glyphs[output.codepoint] = output;
	}

	font.kerning.reserve(glyphs.size());

	for (const auto& left : glyphs) {
		for (const auto& right : glyphs) {
			double advance{ 0.0 };

			if (!font_geometry.getAdvance(advance, left.getCodepoint(), right.getCodepoint())) {
				continue;
			}

			float adjustment{ static_cast<float>(advance - left.getAdvance()) };

			if (adjustment == 0.0f) {
				continue;
			}

			auto kerning_key{ ToKerningKey(left.getCodepoint(), right.getCodepoint()) };

			font.kerning[kerning_key] = adjustment;
		}
	}

	return FontAtlasData{ .surface = std::move(surface), .font = std::move(font) };
}

} // namespace

void FontAtlas::Initialize(Renderer& renderer, FontAtlasData&& data) {
	atlas_texture_ = RendererAccessor{ renderer }.CreateTexture(
		data.surface.Data(), TextureDesc{ .size{ data.surface.GetSize() },
										  .format{ kFontAtlasFormat },
										  .params{ kFontAtlasTextureParams } }
	);
	data_ = std::move(data.font);
}

FontAtlas::FontAtlas(
	Renderer& renderer, path font_path, const path& cache_png_path, const FontAtlasInfo& atlas_info
) {
	auto data{ GenerateFontAtlas(std::move(font_path), atlas_info) };

#ifndef __EMSCRIPTEN__
	auto cache_write{ WriteFontCache(GetAbsolutePath(cache_png_path), data) };

	PTGN_ASSERT(
		cache_write.has_value(), "Failed to write font cache path: ", cache_png_path.string(),
		" with error: ", magic_enum::enum_name(cache_write.error())
	);
#endif

	Initialize(renderer, std::move(data));
}

FontAtlas::FontAtlas(Renderer& renderer, path cache_png_path) {
	cache_png_path = GetAbsolutePath(cache_png_path);

	auto data{ LoadFontCache(cache_png_path) };

	PTGN_ASSERT(
		data.has_value(), "Failed to read font cache path: ", cache_png_path.string(),
		" with error: ", magic_enum::enum_name(data.error())
	);

	Initialize(renderer, std::move(data.value()));
}

FontAtlas::FontAtlas(Renderer& renderer, FontBinary font_png) {
	auto data{ LoadFontCache(font_png) };

	PTGN_ASSERT(
		data.has_value(),
		"Failed to read embedded font cache with error: ", magic_enum::enum_name(data.error())
	);

	Initialize(renderer, std::move(data.value()));
}

std::optional<GlyphMetrics> FontAtlas::GetGlyph(std::uint32_t codepoint) const {
	auto it{ data_.glyphs.find(codepoint) };
	if (it == data_.glyphs.end()) {
		return std::nullopt;
	}
	return it->second;
}

float FontAtlas::GetAdvance(std::uint32_t current_codepoint, std::uint32_t next_codepoint) const {
	auto glyph{ GetGlyph(current_codepoint) };

	if (!glyph.has_value()) {
		return 0.0f;
	}

	float advance{ glyph.value().advance };

	if (auto it{ data_.kerning.find(ToKerningKey(current_codepoint, next_codepoint)) };
		it != data_.kerning.end()) {
		advance += it->second;
	}

	return advance;
}

FontMetrics FontAtlas::GetMetrics() const {
	return data_.metrics;
}

TextureId FontAtlas::GetTexture() const {
	return atlas_texture_;
}

V2_int FontAtlas::GetSize() const {
	return atlas_texture_.GetSize();
}

} // namespace ptgn::impl
