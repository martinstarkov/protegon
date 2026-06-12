#include "runtime/graphics/text/font.h"

#include <ecs/ecs.h>
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <msdfgen.h>
#include <msdfgen-ext.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <istream>
#include <limits>
#include <list>
#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/surface.h"
#include "core/log.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "runtime/asset/asset_manager.h"

namespace ptgn {

template <class T>
concept BinarySerializable = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

namespace {

constexpr std::array kExpectedFontCacheMagic{ 'F', 'O', 'N', 'T', 'C', 'A', 'C', 'H' };
constexpr std::uint32_t kExpectedFontCacheVersion{ 8 };
constexpr int kFontAtlasChannelCount{ 4 };
constexpr TextureFormat kFontAtlasFormat{ TextureFormat::RGBA8 };
constexpr TextureParams kFontAtlasTextureParams{ TextureMinFilter::Linear,
												 TextureMagFilter::Linear };

constexpr std::array<std::uint8_t, 8> kPngSignature{ 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
constexpr std::array<char, 4> kFontDataChunkType{ 'p', 't', 'F', 'N' };
constexpr std::array<char, 4> kPngHeaderChunkType{ 'I', 'H', 'D', 'R' };
constexpr std::array<char, 4> kPngEndChunkType{ 'I', 'E', 'N', 'D' };

constexpr std::size_t kPngChunkLengthSize{ 4 };
constexpr std::size_t kPngChunkTypeSize{ 4 };
constexpr std::size_t kPngChunkCrcSize{ 4 };
constexpr std::size_t kPngChunkHeaderSize{ kPngChunkLengthSize + kPngChunkTypeSize };

constexpr std::uint32_t kPngIhdrPayloadSize{ 13 };

constexpr std::size_t kPngIhdrChunkOffset{ kPngSignature.size() };
constexpr std::size_t kPngIhdrTypeOffset{ kPngIhdrChunkOffset + kPngChunkLengthSize };
constexpr std::size_t kPngIhdrTotalSize{ kPngChunkHeaderSize + kPngIhdrPayloadSize +
										 kPngChunkCrcSize };

constexpr std::size_t kFontDataChunkOffset{ kPngIhdrChunkOffset + kPngIhdrTotalSize };
constexpr std::size_t kFontDataChunkTypeOffset{ kFontDataChunkOffset + kPngChunkLengthSize };
constexpr std::size_t kMinimumFontAtlasPngSize{ kFontDataChunkTypeOffset + kPngChunkTypeSize };

using FontAtlasDataType = std::uint8_t;

using AtlasGenerator = msdf_atlas::ImmediateAtlasGenerator<
	float, kFontAtlasChannelCount, msdf_atlas::mtsdfGenerator,
	msdf_atlas::BitmapAtlasStorage<FontAtlasDataType, kFontAtlasChannelCount> >;

struct FontCacheHeader {
	std::array<char, 8> magic{ kExpectedFontCacheMagic };
	std::uint32_t version{ kExpectedFontCacheVersion };
	std::uint32_t glyph_count{ 0 };
	std::uint32_t kerning_count{ 0 };
};

enum class FontCacheError {
	CannotOpen,
	InvalidMagic,
	UnsupportedVersion,
	ReadFailed,
	WriteFailed,
	InvalidPng,
	MissingFontData,
	MissingPngEnd,
	CrcMismatch
};

class MemoryReader {
public:
	explicit MemoryReader(FontBinary binary) : data_{ binary.buffer }, size_{ binary.length } {}

	template <BinarySerializable T>
	bool Read(T& value) {
		if (!CanRead(sizeof(T))) {
			return false;
		}

		std::memcpy(&value, data_ + offset_, sizeof(T));
		offset_ += sizeof(T);

		return true;
	}

	bool ReadString(std::string& s) {
		std::uint64_t size{ 0 };
		if (!Read(size)) {
			return false;
		}

		auto byte_count{ static_cast<std::size_t>(size) };

		if (!CanRead(byte_count)) {
			return false;
		}

		s.assign(reinterpret_cast<const char*>(data_ + offset_), byte_count);
		offset_ += byte_count;

		return true;
	}

private:
	bool CanRead(std::size_t byte_count) const {
		return data_ && offset_ <= size_ && byte_count <= size_ - offset_;
	}

	const std::uint8_t* data_{ nullptr };
	std::size_t size_{ 0 };
	std::size_t offset_{ 0 };
};

class MemoryWriter {
public:
	template <BinarySerializable T>
	void Write(const T& value) {
		auto* first{ reinterpret_cast<const std::uint8_t*>(&value) };
		bytes.insert(bytes.end(), first, first + sizeof(T));
	}

	void WriteString(std::string_view s) {
		auto size{ static_cast<std::uint64_t>(s.size()) };
		Write(size);

		auto* first{ reinterpret_cast<const std::uint8_t*>(s.data()) };
		bytes.insert(bytes.end(), first, first + s.size());
	}

	std::vector<std::uint8_t> bytes;
};

bool ReadPath(MemoryReader& in, path& p) {
	std::string s;
	if (!in.ReadString(s)) {
		return false;
	}

	p = path{ s };
	return true;
}

void WritePath(MemoryWriter& out, const path& p) {
	out.WriteString(p.string());
}

std::uint32_t Crc32(std::span<const std::uint8_t> bytes) {
	std::uint32_t crc{ 0xFFFFFFFFU };

	for (auto byte : bytes) {
		crc ^= byte;

		for (int bit{ 0 }; bit < 8; ++bit) {
			crc = (crc & 1U) ? (crc >> 1U) ^ 0xEDB88320U : crc >> 1U;
		}
	}

	return crc ^ 0xFFFFFFFFU;
}

bool HasPngSignature(std::span<const std::uint8_t> bytes) {
	return bytes.size() >= kPngSignature.size() &&
		   std::ranges::equal(kPngSignature, bytes.first(kPngSignature.size()));
}

std::uint32_t ReadBigEndianU32(std::span<const std::uint8_t> bytes, std::size_t offset) {
	return (static_cast<std::uint32_t>(bytes[offset + 0]) << 24U) |
		   (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U) |
		   (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U) |
		   static_cast<std::uint32_t>(bytes[offset + 3]);
}

void AppendBigEndianU32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
	bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
	bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
	bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
	bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

bool MatchesType(
	std::span<const std::uint8_t> bytes, std::size_t offset, std::array<char, 4> type
) {
	for (std::size_t i{ 0 }; i < type.size(); ++i) {
		if (bytes[offset + i] != static_cast<std::uint8_t>(type[i])) {
			return false;
		}
	}

	return true;
}

bool HasValidPngHeaderChunk(std::span<const std::uint8_t> png_bytes) {
	if (png_bytes.size() < kMinimumFontAtlasPngSize) {
		return false;
	}

	if (!HasPngSignature(png_bytes)) {
		return false;
	}

	if (ReadBigEndianU32(png_bytes, kPngIhdrChunkOffset) != kPngIhdrPayloadSize) {
		return false;
	}

	return MatchesType(png_bytes, kPngIhdrTypeOffset, kPngHeaderChunkType);
}

bool IsFontAtlasPng(std::span<const std::uint8_t> png_bytes) {
	return HasValidPngHeaderChunk(png_bytes) &&
		   MatchesType(png_bytes, kFontDataChunkTypeOffset, kFontDataChunkType);
}

std::expected<FontBinary, FontCacheError> GetExpectedFontDataChunkPayload(
	std::span<const std::uint8_t> png_bytes
) {
	if (!HasValidPngHeaderChunk(png_bytes)) {
		return std::unexpected{ FontCacheError::InvalidPng };
	}

	if (!MatchesType(png_bytes, kFontDataChunkTypeOffset, kFontDataChunkType)) {
		return std::unexpected{ FontCacheError::MissingFontData };
	}

	auto length{ ReadBigEndianU32(png_bytes, kFontDataChunkOffset) };
	auto data_offset{ kFontDataChunkOffset + kPngChunkHeaderSize };
	auto crc_offset{ data_offset + static_cast<std::size_t>(length) };
	auto chunk_end{ crc_offset + kPngChunkCrcSize };

	if (chunk_end > png_bytes.size()) {
		return std::unexpected{ FontCacheError::InvalidPng };
	}

	auto stored_crc{ ReadBigEndianU32(png_bytes, crc_offset) };

	std::span<const std::uint8_t> crc_bytes{ png_bytes.data() + kFontDataChunkTypeOffset,
											 kPngChunkTypeSize + static_cast<std::size_t>(length) };

	auto computed_crc{ Crc32(crc_bytes) };

	if (stored_crc != computed_crc) {
		return std::unexpected{ FontCacheError::CrcMismatch };
	}

	return FontBinary{ png_bytes.data() + data_offset, static_cast<std::size_t>(length) };
}

std::vector<std::uint8_t> WriteFontCachePayload(const impl::FontData& font) {
	MemoryWriter out;

	FontCacheHeader header{ .glyph_count   = static_cast<std::uint32_t>(font.glyphs.size()),
							.kerning_count = static_cast<std::uint32_t>(font.kerning.size()) };

	out.Write(header);
	WritePath(out, font.font_path);
	out.Write(font.metrics);

	for (const auto& [_, glyph] : font.glyphs) {
		out.Write(glyph);
	}

	for (const auto& [key, value] : font.kerning) {
		out.Write(key);
		out.Write(value);
	}

	return std::move(out.bytes);
}

std::expected<impl::FontData, FontCacheError> ReadFontCachePayload(FontBinary binary) {
	MemoryReader in{ binary };

	FontCacheHeader header;
	if (!in.Read(header)) {
		return std::unexpected(FontCacheError::ReadFailed);
	}

	if (header.magic != kExpectedFontCacheMagic) {
		return std::unexpected(FontCacheError::InvalidMagic);
	}

	if (header.version != kExpectedFontCacheVersion) {
		return std::unexpected(FontCacheError::UnsupportedVersion);
	}

	impl::FontData font;

	if (!ReadPath(in, font.font_path)) {
		return std::unexpected(FontCacheError::ReadFailed);
	}
	if (!in.Read(font.metrics)) {
		return std::unexpected(FontCacheError::ReadFailed);
	}

	font.glyphs.reserve(header.glyph_count);

	for (std::uint32_t i{ 0 }; i < header.glyph_count; ++i) {
		impl::GlyphMetrics glyph;
		if (!in.Read(glyph)) {
			return std::unexpected(FontCacheError::ReadFailed);
		}

		font.glyphs.try_emplace(glyph.codepoint, glyph);
	}

	font.kerning.reserve(header.kerning_count);

	for (std::uint32_t i{ 0 }; i < header.kerning_count; ++i) {
		std::uint64_t key{ 0 };
		float value{ 0.0f };

		if (!in.Read(key)) {
			return std::unexpected(FontCacheError::ReadFailed);
		}
		if (!in.Read(value)) {
			return std::unexpected(FontCacheError::ReadFailed);
		}

		font.kerning.emplace(key, value);
	}

	return font;
}

std::vector<std::uint8_t> MakePngChunk(
	std::array<char, 4> type, std::span<const std::uint8_t> payload
) {
	PTGN_ASSERT(
		payload.size() <= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()),
		"PNG chunk payload is too large"
	);

	std::vector<std::uint8_t> chunk;
	chunk.reserve(12 + payload.size());

	AppendBigEndianU32(chunk, static_cast<std::uint32_t>(payload.size()));

	for (auto c : type) {
		chunk.push_back(static_cast<std::uint8_t>(c));
	}

	chunk.insert(chunk.end(), payload.begin(), payload.end());

	auto crc_start{ chunk.begin() + 4 };
	auto crc{ Crc32(std::span<const std::uint8_t>{ crc_start, chunk.end() }) };

	AppendBigEndianU32(chunk, crc);

	return chunk;
}

impl::Surface CreateSurfaceFromEncodedPng(FontBinary font_png) {
	return impl::Surface{ std::span{ font_png.buffer, font_png.length }, kFontAtlasChannelCount };
}

FontCacheError ToFontCacheError(FileWriteError error) {
	switch (error) {
		case FileWriteError::OpenFailed:  return FontCacheError::CannotOpen;
		case FileWriteError::WriteFailed: return FontCacheError::WriteFailed;
		default:						  PTGN_ERROR("Unknown FileWriteError: ", std::to_underlying(error));
	}
}

std::expected<impl::FontData, FontCacheError> ReadFontCacheFromPng(FontBinary font_png) {
	if (!font_png.buffer || font_png.length == 0) {
		return std::unexpected{ FontCacheError::InvalidPng };
	}

	std::span<const std::uint8_t> png_bytes{ font_png.buffer, font_png.length };

	auto payload{ GetExpectedFontDataChunkPayload(png_bytes) };
	if (!payload.has_value()) {
		return std::unexpected{ payload.error() };
	}

	return ReadFontCachePayload(payload.value());
}

std::expected<impl::FontData, FontCacheError> ReadFontCacheFromPng(const path& png_path) {
	auto png_bytes{ ReadBinary(png_path) };
	return ReadFontCacheFromPng(FontBinary{ png_bytes.data(), png_bytes.size() });
}

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

auto MakeCharset(std::uint32_t begin, std::uint32_t end) {
	msdf_atlas::Charset charset;
	for (std::uint32_t c{ begin }; c <= end; ++c) {
		charset.add(c);
	}
	return charset;
}

std::uint64_t KerningKey(std::uint32_t current_codepoint, std::uint32_t next_codepoint) {
	return (static_cast<std::uint64_t>(current_codepoint) << 32ULL) |
		   static_cast<std::uint64_t>(next_codepoint);
}

std::expected<std::vector<std::uint8_t>, FontCacheError> InsertPngChunkAfterIhdr(
	std::span<const std::uint8_t> png_bytes, std::array<char, 4> type,
	std::span<const std::uint8_t> payload
) {
	if (!HasValidPngHeaderChunk(png_bytes)) {
		return std::unexpected{ FontCacheError::InvalidPng };
	}

	auto inserted_chunk{ MakePngChunk(type, payload) };

	std::vector<std::uint8_t> output;
	output.reserve(png_bytes.size() + inserted_chunk.size());

	output.insert(output.end(), png_bytes.begin(), png_bytes.begin() + kPngSignature.size());

	std::size_t offset{ kPngSignature.size() };
	bool inserted{ false };

	while (offset + kPngChunkHeaderSize + kPngChunkCrcSize <= png_bytes.size()) {
		auto length{ ReadBigEndianU32(png_bytes, offset) };
		auto chunk_size{ kPngChunkHeaderSize + static_cast<std::size_t>(length) +
						 kPngChunkCrcSize };

		if (offset + chunk_size > png_bytes.size()) {
			return std::unexpected{ FontCacheError::InvalidPng };
		}

		auto type_offset{ offset + kPngChunkLengthSize };
		auto is_existing_target_chunk{ MatchesType(png_bytes, type_offset, type) };

		// Remove older copies of the same custom chunk when regenerating.
		if (!is_existing_target_chunk) {
			output.insert(
				output.end(), png_bytes.begin() + offset, png_bytes.begin() + offset + chunk_size
			);
		}

		if (!inserted && MatchesType(png_bytes, type_offset, kPngHeaderChunkType)) {
			output.insert(output.end(), inserted_chunk.begin(), inserted_chunk.end());
			inserted = true;
		}

		if (MatchesType(png_bytes, type_offset, kPngEndChunkType)) {
			break;
		}

		offset += chunk_size;
	}

	if (!inserted) {
		return std::unexpected{ FontCacheError::InvalidPng };
	}

	return output;
}

} // namespace

namespace impl {

bool IsFontAtlasPng(const path& png_path) {
	std::ifstream in{ GetAbsolutePath(png_path), std::ios::binary };
	if (!in) {
		return false;
	}

	std::array<std::uint8_t, kMinimumFontAtlasPngSize> bytes{};
	in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

	if (in.gcount() != static_cast<std::streamsize>(bytes.size())) {
		return false;
	}

	return ptgn::IsFontAtlasPng(bytes);
}

FontObject::FontObject(
	const AssetManager& asset_manager, path font_path, path cache_png_path,
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
	packer.setUnitRange(atlas_info.em_range);
	// packer.setPixelRange(atlas_info.pixel_range);
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

	atlas_texture_ =
		asset_manager.CreateTexture(surface, kFontAtlasFormat, kFontAtlasTextureParams);

	data_.font_path = std::move(font_path);

	const auto& msdf_metrics{ font_geometry.getMetrics() };
	data_.metrics.ascender	  = static_cast<float>(msdf_metrics.ascenderY);
	data_.metrics.descender	  = static_cast<float>(msdf_metrics.descenderY);
	data_.metrics.line_height = static_cast<float>(msdf_metrics.lineHeight);
	data_.metrics.em_size	  = static_cast<float>(packer.getScale());
	data_.metrics.pixel_range = atlas_info.em_range * data_.metrics.em_size;
	// data_.metrics.pixel_range = atlas_info.pixel_range;
	PTGN_ASSERT(
		data_.metrics.em_size == atlas_info.em_size,
		"Failed to create font with em size: ", atlas_info.em_size
	);

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

		out.uv = Rect{ { atlas_left / bitmap.width, atlas_top / bitmap.height },
					   { atlas_right / bitmap.width, atlas_bottom / bitmap.height } };

		out.uv.min.y = 1.0f - out.uv.min.y;
		out.uv.max.y = 1.0f - out.uv.max.y;

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
	cache_png_path = GetAbsolutePath(cache_png_path);

	auto png_bytes{ surface.EncodePNG() };
	auto font_payload{ WriteFontCachePayload(data_) };
	auto font_png{ InsertPngChunkAfterIhdr(png_bytes, kFontDataChunkType, font_payload) };

	PTGN_ASSERT(
		font_png.has_value(),
		"Failed to insert font data into png with error: ", magic_enum::enum_name(font_png.error())
	);

	auto cache_write{ WriteBinary(cache_png_path, font_png.value()) };

	PTGN_ASSERT(
		cache_write.has_value(),
		"Failed to write embedded font png cache path: ", cache_png_path.string(),
		" with error: ", magic_enum::enum_name(cache_write.error())
	);
#endif
}

FontObject::FontObject(const AssetManager& asset_manager, path cache_png_path) {
	cache_png_path = GetAbsolutePath(cache_png_path);

	Surface surface{ cache_png_path, kFontAtlasChannelCount };

	atlas_texture_ =
		asset_manager.CreateTexture(surface, kFontAtlasFormat, kFontAtlasTextureParams);

	auto cache_read{ ReadFontCacheFromPng(cache_png_path) };
	PTGN_ASSERT(
		cache_read.has_value(),
		"Failed to read embedded font data from png cache path: ", cache_png_path.string(),
		" with error: ", magic_enum::enum_name(cache_read.error())
	);

	data_ = std::move(cache_read.value());
}

FontObject::FontObject(const AssetManager& asset_manager, FontBinary font_png) {
	auto surface{ CreateSurfaceFromEncodedPng(font_png) };

	atlas_texture_ =
		asset_manager.CreateTexture(surface, kFontAtlasFormat, kFontAtlasTextureParams);

	auto cache_read{ ReadFontCacheFromPng(font_png) };
	PTGN_ASSERT(
		cache_read.has_value(), "Failed to read embedded default font data from png with error: ",
		magic_enum::enum_name(cache_read.error())
	);

	data_ = std::move(cache_read.value());
}

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

	float advance{ glyph.value().advance };

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
