#include "runtime/graphics/text/font_data.h"

#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <msdfgen.h>
#include <msdfgen-ext.h>

#include <cstdint>
#include <filesystem>
#include <list>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/surface.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"

namespace ptgn::impl {

using AtlasGenerator = msdf_atlas::ImmediateAtlasGenerator<
	float, 3, msdf_atlas::msdfGenerator, msdf_atlas::BitmapAtlasStorage<std::uint8_t, 3> >;

static msdf_atlas::Charset MakeCharset(std::uint32_t begin, std::uint32_t end) {
	msdf_atlas::Charset charset;
	for (std::uint32_t c{ begin }; c <= end; ++c) {
		charset.add(c);
	}
	return charset;
}

MsdfFontData::MsdfFontData(
	Renderer& renderer, const path& font_path, std::uint32_t atlas_texture_index,
	CreateInfo create_info
) :
	font_id_{ ptgn::Hash(font_path.string()) },
	atlas_texture_index_{ atlas_texture_index },
	pixel_range_{ create_info.pixel_range },
	em_size_{ create_info.em_size } {
	msdfgen::FreetypeHandle* freetype{ msdfgen::initializeFreetype() };
	if (freetype == nullptr) {
		throw std::runtime_error{ "Failed to initialize FreeType for MSDF font generation" };
	}

	auto abs_path{ GetAbsolutePath("assets/Arial.ttf") };

	msdfgen::FontHandle* font{ msdfgen::loadFont(freetype, abs_path.string().c_str()) };

	if (font == nullptr) {
		msdfgen::deinitializeFreetype(freetype);
		throw std::runtime_error{ "Failed to load font: " + abs_path.string() };
	}

	std::vector<msdf_atlas::GlyphGeometry> glyphs;
	msdf_atlas::FontGeometry font_geometry{ &glyphs };

	msdf_atlas::Charset charset{ MakeCharset(create_info.charset_begin, create_info.charset_end) };

	const int loaded_glyphs{ font_geometry.loadCharset(font, 1.0, charset) };

	PTGN_ASSERT(loaded_glyphs > 0, "Failed to load glyphs from font: ", abs_path.string());

	std::uint64_t glyph_seed{ 0 };
	for (auto& glyph : glyphs) {
		glyph_seed = glyph_seed * 6364136223846793005ULL + 1442695040888963407ULL;
		glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, create_info.max_corner_angle, glyph_seed);
	}

	msdf_atlas::TightAtlasPacker packer;
	packer.setDimensionsConstraint(msdf_atlas::DimensionsConstraint::SQUARE);
	packer.setScale(create_info.em_size);
	packer.setPixelRange(create_info.pixel_range);
	packer.setMiterLimit(1.0);

	const int remaining{ packer.pack(glyphs.data(), static_cast<int>(glyphs.size())) };

	PTGN_ASSERT(
		remaining == 0, "Failed to pack all MSDF glyphs into atlas for font: ", abs_path.string()
	);

	int atlas_width{ 0 };
	int atlas_height{ 0 };
	packer.getDimensions(atlas_width, atlas_height);

	msdf_atlas::GeneratorAttributes attributes;
	attributes.config.overlapSupport = true;
	attributes.scanlinePass			 = true;

	AtlasGenerator generator{ atlas_width, atlas_height };
	generator.setAttributes(attributes);
	generator.setThreadCount(static_cast<int>(create_info.thread_count));
	generator.generate(glyphs.data(), static_cast<int>(glyphs.size()));

	constexpr int channel_count{ 3 };

	msdfgen::BitmapConstRef<std::uint8_t, channel_count> bitmap{ generator.atlasStorage() };

	atlas_size_ = { bitmap.width, bitmap.height };

	auto count{ static_cast<std::size_t>(bitmap.width) * static_cast<std::size_t>(bitmap.height) *
				channel_count };

	Surface surface{ atlas_size_, std::span<const std::uint8_t>{ bitmap.pixels, count },
					 channel_count, true };

	TextureObject texture{ renderer.CreateTexture(surface, TextureFormat::RGB8) };

	atlas_texture_ = std::move(texture);

	const auto& msdf_metrics{ font_geometry.getMetrics() };
	metrics_.ascender	 = static_cast<float>(msdf_metrics.ascenderY);
	metrics_.descender	 = static_cast<float>(msdf_metrics.descenderY);
	metrics_.line_height = static_cast<float>(msdf_metrics.lineHeight);

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

		GlyphMetrics out;
		out.codepoint = glyph.getCodepoint();
		out.advance	  = static_cast<float>(glyph.getAdvance());

		out.plane = Rect{ { static_cast<float>(plane_left), -static_cast<float>(plane_top) },
						  { static_cast<float>(plane_right), -static_cast<float>(plane_bottom) } };

		out.uv =
			Rect{ { static_cast<float>(atlas_left) / static_cast<float>(bitmap.width),
					1.0f - static_cast<float>(atlas_top) / static_cast<float>(bitmap.height) },
				  { static_cast<float>(atlas_right) / static_cast<float>(bitmap.width),
					1.0f - static_cast<float>(atlas_bottom) / static_cast<float>(bitmap.height) } };

		glyphs_[out.codepoint] = out;
	}

	for (const auto& left : glyphs) {
		for (const auto& right : glyphs) {
			double advance{};
			if (font_geometry.getAdvance(advance, left.getCodepoint(), right.getCodepoint())) {
				const float adjustment{ static_cast<float>(advance - left.getAdvance()) };

				if (adjustment != 0.0f) {
					kerning_[KerningKey(
						static_cast<std::uint32_t>(left.getCodepoint()),
						static_cast<std::uint32_t>(right.getCodepoint())
					)] = adjustment;
				}
			}
		}
	}

	msdfgen::destroyFont(font);
	msdfgen::deinitializeFreetype(freetype);
}

MsdfFontData::MsdfFontData(
	std::uint64_t font_id, TextureObject&& atlas_texture, std::uint32_t atlas_texture_index,
	V2_int atlas_size, FontMetrics metrics
) :
	font_id_{ font_id },
	atlas_texture_{ std::move(atlas_texture) },
	atlas_texture_index_{ atlas_texture_index },
	atlas_size_{ atlas_size },
	metrics_{ metrics } {}

std::optional<GlyphMetrics> MsdfFontData::GetGlyph(std::uint32_t codepoint) const {
	auto it{ glyphs_.find(codepoint) };
	if (it == glyphs_.end()) {
		return std::nullopt;
	}
	return it->second;
}

float MsdfFontData::GetAdvance(std::uint32_t current_codepoint, std::uint32_t next_codepoint)
	const {
	auto glyph{ GetGlyph(current_codepoint) };
	if (!glyph.has_value()) {
		return 0.0f;
	}

	float advance{ glyph->advance };

	if (auto it{ kerning_.find(KerningKey(current_codepoint, next_codepoint)) };
		it != kerning_.end()) {
		advance += it->second;
	}

	return advance;
}

FontMetrics MsdfFontData::GetFontMetrics() const {
	return metrics_;
}

std::uint64_t MsdfFontData::GetFontId() const {
	return font_id_;
}

std::uint32_t MsdfFontData::GetAtlasTextureIndex() const {
	return atlas_texture_index_;
}

TextureId MsdfFontData::GetAtlasTexture() const {
	return atlas_texture_;
}

V2_int MsdfFontData::GetAtlasSize() const {
	return atlas_size_;
}

float MsdfFontData::GetPixelRange() const {
	return pixel_range_;
}

float MsdfFontData::GetEmSize() const {
	return em_size_;
}

std::uint64_t MsdfFontData::KerningKey(
	std::uint32_t current_codepoint, std::uint32_t next_codepoint
) {
	return (static_cast<std::uint64_t>(current_codepoint) << 32ULL) |
		   static_cast<std::uint64_t>(next_codepoint);
}

} // namespace ptgn::impl
