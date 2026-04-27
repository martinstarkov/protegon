#include "runtime/graphics/text/font.h"

#include <ecs/ecs.h>
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <msdfgen.h>
#include <msdfgen-ext.h>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <istream>
#include <list>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/surface.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

namespace impl {

constexpr std::array<char, 8> kExpectedFontCacheMagic{ 'F', 'O', 'N', 'T', 'C', 'A', 'C', 'H' };
constexpr std::uint32_t kExpectedFontCacheVersion{ 1 };
constexpr int kFontAtlasChannelCount{ 3 };
constexpr TextureFormat kFontAtlasFormat{ TextureFormat::RGB8 };
constexpr TextureParameters kFontAtlasTextureParams{ TextureMinFilter::Linear,
													 TextureMagFilter::Linear };

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
	std::uint32_t version{ kExpectedFontCacheVersion };
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

static bool WriteString(std::ofstream& out, std::string_view s) {
	std::uint64_t size = s.size();
	WriteRaw(out, size);
	out.write(s.data(), static_cast<std::streamsize>(size));
	return static_cast<bool>(out);
}

static bool ReadString(std::ifstream& in, std::string& s) {
	std::uint64_t size{};
	ReadRaw(in, size);
	s.resize(size);
	in.read(s.data(), static_cast<std::streamsize>(size));
	return static_cast<bool>(in);
}

static bool WritePath(std::ofstream& out, const path& p) {
	return WriteString(out, p.string());
}

static bool ReadPath(std::ifstream& in, path& p) {
	std::string s;
	if (!ReadString(in, s)) {
		return false;
	}
	p = path{ s };
	return true;
}

[[nodiscard]] static std::expected<void, FontCacheError> WriteFontCache(
	const path& cache_path, const FontData& font
) {
	EnsureDirectory(cache_path.parent_path());

	std::ofstream out(cache_path, std::ios::binary);
	if (!out) {
		return std::unexpected(FontCacheError::CannotOpen);
	}

	if (FontCacheHeader header{ .glyph_count   = static_cast<std::uint32_t>(font.glyphs.size()),
								.kerning_count = static_cast<std::uint32_t>(font.kerning.size()) };
		!WriteRaw(out, header)) {
		return std::unexpected(FontCacheError::WriteFailed);
	}
	if (!WritePath(out, font.font_path)) {
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

[[nodiscard]] static std::expected<FontData, FontCacheError> ReadFontCache(const path& cache_path) {
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

	if (header.version != kExpectedFontCacheVersion) {
		return std::unexpected(FontCacheError::UnsupportedVersion);
	}

	FontData font;

	if (!ReadPath(in, font.font_path)) {
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
	generator.setThreadCount(atlas_info.thread_count);
	generator.generate(glyphs.data(), static_cast<int>(glyphs.size()));

	msdfgen::BitmapConstRef<FontAtlasDataType, kFontAtlasChannelCount> bitmap{
		generator.atlasStorage()
	};

	atlas_size = { bitmap.width, bitmap.height };

	auto byte_count{ static_cast<std::size_t>(bitmap.width) *
					 static_cast<std::size_t>(bitmap.height) * kFontAtlasChannelCount };

	Surface surface{ atlas_size, std::span<const FontAtlasDataType>{ bitmap.pixels, byte_count },
					 kFontAtlasChannelCount, true };

	cache_directory = GetAbsolutePath(cache_directory);

	atlas_texture_ = renderer.CreateTexture(surface, kFontAtlasFormat, kFontAtlasTextureParams);

	data_.font_path = std::move(font_path);

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

#ifndef __EMSCRIPTEN__
	auto cache_png_path{ cache_directory / (std::string(cache_name) + ".png") };
	auto cache_data_path{ cache_directory / (std::string(cache_name) + ".data") };

	auto success{ surface.SavePNG(cache_png_path) };

	PTGN_ASSERT(
		success.has_value(), "Failed to cache font atlas as png to path: ", cache_png_path.string()
	);

	auto cache_write{ WriteFontCache(cache_data_path, data_) };
	PTGN_ASSERT(
		cache_write.has_value(),
		"Failed to write font data to cache path: ", cache_data_path.string(),
		" with error: ", magic_enum::enum_name(cache_write.error())
	);
#endif
}

#ifndef __EMSCRIPTEN__
FontObject::FontObject(Renderer& renderer, path cache_directory, std::string_view cache_name) {
	cache_directory = GetAbsolutePath(cache_directory);

	auto cache_png_path{ cache_directory / (std::string(cache_name) + ".png") };

	Surface surface{ cache_png_path, kFontAtlasChannelCount };

	atlas_texture_ = renderer.CreateTexture(surface, kFontAtlasFormat, kFontAtlasTextureParams);

	auto cache_data_path{ cache_directory / (std::string(cache_name) + ".data") };

	auto cache_read{ ReadFontCache(cache_data_path) };
	PTGN_ASSERT(
		cache_read.has_value(),
		"Failed to read font data from cache path: ", cache_data_path.string(),
		" with error: ", magic_enum::enum_name(cache_read.error())
	);

	data_ = std::move(cache_read.value());
}
#endif

std::optional<GlyphMetrics> FontObject::GetGlyph(std::uint32_t codepoint) const {
	auto it{ data_.glyphs.find(codepoint) };
	if (it == data_.glyphs.end()) {
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

	if (auto it{ data_.kerning.find(KerningKey(current_codepoint, next_codepoint)) };
		it != data_.kerning.end()) {
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

V2_int FontObject::GetAtlasSize() const {
	return atlas_texture_.GetSize();
}

} // namespace impl

std::optional<impl::GlyphMetrics> Font::GetGlyph(std::uint32_t codepoint) const {
	return GetEntity().Get<impl::FontObject>().GetGlyph(codepoint);
}

float Font::GetAdvance(std::uint32_t current_codepoint, std::uint32_t next_codepoint) const {
	return GetEntity().Get<impl::FontObject>().GetAdvance(current_codepoint, next_codepoint);
}

const impl::FontData& Font::GetFontData() const {
	return GetEntity().Get<impl::FontObject>().GetFontData();
}

impl::TextureId Font::GetAtlasTexture() const {
	return GetEntity().Get<impl::FontObject>().GetAtlasTexture();
}

V2_int Font::GetAtlasSize() const {
	return GetEntity().Get<impl::FontObject>().GetAtlasSize();
}

} // namespace ptgn
