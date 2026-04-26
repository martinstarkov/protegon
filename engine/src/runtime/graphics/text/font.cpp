#include "runtime/graphics/text/font.h"

#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <msdfgen.h>
#include <msdfgen-ext.h>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <istream>
#include <list>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
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

constexpr std::array<char, 8> kExpectedFontCacheMagic{ 'F', 'O', 'N', 'T', 'C', 'A', 'C', 'H' };
constexpr int kFontAtlasChannelCount{ 3 };
constexpr TextureFormat kFontAtlasFormat{ TextureFormat::RGB8 };

using FontAtlasDataType = std::uint8_t;

using AtlasGenerator = msdf_atlas::ImmediateAtlasGenerator<
	float, kFontAtlasChannelCount, msdf_atlas::msdfGenerator,
	msdf_atlas::BitmapAtlasStorage<FontAtlasDataType, kFontAtlasChannelCount> >;

static auto InitFreetype() {
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

static auto LoadFont(auto& freetype, const path& font_path) {
	auto font{ msdfgen::loadFont(freetype.get(), font_path.string().c_str()) };
	PTGN_ASSERT(font, "Failed to load font from: ", font_path.string());
	return std::unique_ptr<msdfgen::FontHandle, void (*)(msdfgen::FontHandle*)>{
		font,
		[](msdfgen::FontHandle* f) {
			if (f) {
				msdfgen::destroyFont(f);
			}
		}
	};
}

static msdf_atlas::Charset MakeCharset(std::uint32_t begin, std::uint32_t end) {
	msdf_atlas::Charset charset;
	for (std::uint32_t c{ begin }; c <= end; ++c) {
		charset.add(c);
	}
	return charset;
}

static std::uint64_t KerningKey(std::uint32_t current_codepoint, std::uint32_t next_codepoint) {
	return (static_cast<std::uint64_t>(current_codepoint) << 32ULL) |
		   static_cast<std::uint64_t>(next_codepoint);
}

enum class FontCacheError {
	CannotOpen,
	InvalidMagic,
	UnsupportedVersion,
	ReadFailed,
	WriteFailed
};

struct FontCacheHeader {
	std::array<char, 8> magic{ kExpectedFontCacheMagic };
	std::uint32_t version{ 1 };
	std::uint32_t glyph_count{ 0 };
	std::uint32_t kerning_count{ 0 };
};

template <class T>
concept BinarySerializable = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

template <BinarySerializable T>
bool WriteRaw(std::ofstream& out, const T& value) {
	out.write(reinterpret_cast<const char*>(&value), sizeof(T));
	return static_cast<bool>(out);
}

template <BinarySerializable T>
bool ReadRaw(std::ifstream& in, T& value) {
	in.read(reinterpret_cast<char*>(&value), sizeof(T));
	return static_cast<bool>(in);
}

[[nodiscard]] static std::expected<void, FontCacheError> WriteFontCache(
	const path& cache_path, const FontData& font
) {
	std::ofstream out(cache_path, std::ios::binary);
	if (!out) {
		return std::unexpected(FontCacheError::CannotOpen);
	}

	if (FontCacheHeader header{ .glyph_count   = static_cast<std::uint32_t>(font.glyphs.size()),
								.kerning_count = static_cast<std::uint32_t>(font.kerning.size()) };
		!WriteRaw(out, header)) {
		return std::unexpected(FontCacheError::WriteFailed);
	}
	// TODO: Fix.
	if (!WriteRaw(out, font.font_path.string())) {
		return std::unexpected(FontCacheError::WriteFailed);
	}
	if (!WriteRaw(out, font.metrics)) {
		return std::unexpected(FontCacheError::WriteFailed);
	}

	for (const auto& [_, glyph] : font.glyphs) {
		if (!WriteRaw(out, glyph)) {
			return std::unexpected(FontCacheError::WriteFailed);
		}
	}

	for (const auto& [key, value] : font.kerning) {
		if (!WriteRaw(out, key)) {
			return std::unexpected(FontCacheError::WriteFailed);
		}
		if (!WriteRaw(out, value)) {
			return std::unexpected(FontCacheError::WriteFailed);
		}
	}

	return {};
}

[[nodiscard]] static std::expected<FontData, FontCacheError> ReadFontCache(
	const fs::path& cache_path
) {
	std::ifstream in(cache_path, std::ios::binary);
	if (!in) {
		return std::unexpected(FontCacheError::CannotOpen);
	}

	FontCacheHeader header;
	if (!ReadRaw(in, header)) {
		return std::unexpected(FontCacheError::ReadFailed);
	}

	if (header.magic != kExpectedFontCacheMagic) {
		return std::unexpected(FontCacheError::InvalidMagic);
	}

	if (header.version != 1) {
		return std::unexpected(FontCacheError::UnsupportedVersion);
	}

	FontData font;

	// TODO: Fix.
	if (!ReadRaw(in, font.font_path)) {
		return std::unexpected(FontCacheError::WriteFailed);
	}
	if (!ReadRaw(in, font.metrics)) {
		return std::unexpected(FontCacheError::ReadFailed);
	}

	font.glyphs.reserve(header.glyph_count);

	for (std::uint32_t i{ 0 }; i < header.glyph_count; ++i) {
		GlyphMetrics glyph;
		if (!ReadRaw(in, glyph)) {
			return std::unexpected(FontCacheError::ReadFailed);
		}

		font.glyphs.try_emplace(glyph.codepoint, glyph);
	}

	font.kerning.reserve(header.kerning_count);

	for (std::uint32_t i{ 0 }; i < header.kerning_count; ++i) {
		std::uint64_t key{ 0 };
		float value{ 0.0f };

		if (!ReadRaw(in, key)) {
			return std::unexpected(FontCacheError::ReadFailed);
		}

		if (!ReadRaw(in, value)) {
			return std::unexpected(FontCacheError::ReadFailed);
		}

		font.kerning.emplace(key, value);
	}

	return font;
}

FontObject::FontObject(
	Renderer& renderer, path font_path, path cache_directory, std::string_view cache_name,
	const FontAtlasInfo& atlas_info
) {
	auto freetype{ InitFreetype() };

	font_path = GetAbsolutePath(font_path);

	auto font{ LoadFont(freetype, font_path) };

	std::vector<msdf_atlas::GlyphGeometry> glyphs;
	msdf_atlas::FontGeometry font_geometry{ &glyphs };

	auto charset{ MakeCharset(atlas_info.charset_begin, atlas_info.charset_end) };

	constexpr float kFontScale{ 1.0f };

	auto loaded_glyph_count{ font_geometry.loadCharset(font.get(), kFontScale, charset) };
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
	packer.setPixelRange(atlas_info.pixel_range);
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
	auto thread_count{ std::thread::hardware_concurrency() };
	generator.setThreadCount(static_cast<int>(thread_count));
	generator.generate(glyphs.data(), static_cast<int>(glyphs.size()));

	msdfgen::BitmapConstRef<FontAtlasDataType, kFontAtlasChannelCount> bitmap{
		generator.atlasStorage()
	};

	atlas_size = { bitmap.width, bitmap.height };

	auto byte_count{ static_cast<std::size_t>(bitmap.width) *
					 static_cast<std::size_t>(bitmap.height) * kFontAtlasChannelCount };

	Surface surface{ atlas_size, std::span<const FontAtlasDataType>{ bitmap.pixels, byte_count },
					 kFontAtlasChannelCount, true };

	auto success{ surface.SavePNG(cache_path) };

	PTGN_ASSERT(
		success.has_value(), "Failed to cache font atlas as png to path: ", cache_path.string()
	);

	auto texture{ renderer.CreateTexture(surface, kFontAtlasFormat) };

	atlas_texture_ = std::move(texture);

	const auto& msdf_metrics{ font_geometry.getMetrics() };
	data_.metrics.ascender	  = static_cast<float>(msdf_metrics.ascenderY);
	data_.metrics.descender	  = static_cast<float>(msdf_metrics.descenderY);
	data_.metrics.line_height = static_cast<float>(msdf_metrics.lineHeight);

	data_.glyphs.reserve(glyphs.size());

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

		out.plane = Rect{ { plane_left, -plane_top }, { plane_right, -plane_bottom } };

		out.uv = Rect{ { atlas_left / bitmap.width, (1.0 - atlas_top) / bitmap.height },
					   { atlas_right / bitmap.width, (1.0 - atlas_bottom) / bitmap.height } };

		data_.glyphs[out.codepoint] = out;
	}

	data_.kerning.reserve(glyphs.size());

	for (const auto& left : glyphs) {
		for (const auto& right : glyphs) {
			double advance{ 0.0 };
			if (!font_geometry.getAdvance(advance, left.getCodepoint(), right.getCodepoint())) {
				continue;
			}

			float adjustment{ static_cast<float>(advance - left.getAdvance()) };

			if (adjustment != 0.0f) {
				auto kerning_key{ KerningKey(left.getCodepoint(), right.getCodepoint()) };
				data_.kerning[kerning_key] = adjustment;
			}
		}
	}
}

FontObject::FontObject(Renderer& renderer, path cache_directory, std::string_view cache_name) {}

std::optional<GlyphMetrics> FontObject::GetGlyph(std::uint32_t codepoint) const {
	auto it{ glyphs_.find(codepoint) };
	if (it == glyphs_.end()) {
		return std::nullopt;
	}
	return it->second;
}

float FontObject::GetAdvance(std::uint32_t current_codepoint, std::uint32_t next_codepoint) const {
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

const FontData& FontObject::GetFontData() const {
	return data_;
}

TextureId FontObject::GetAtlasTexture() const {
	return atlas_texture_;
}

} // namespace ptgn::impl
