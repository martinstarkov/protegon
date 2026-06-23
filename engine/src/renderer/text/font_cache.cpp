#include "renderer/text/font_cache.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <istream>
#include <limits>
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
#include "core/util/file.h"
#include "renderer/text/font_atlas.h"

namespace ptgn::impl {

namespace {

template <class T>
concept BinarySerializable = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

constexpr std::array kExpectedFontCacheMagic{ 'F', 'O', 'N', 'T', 'C', 'A', 'C', 'H' };
constexpr std::uint32_t kExpectedFontCacheVersion{ 9 };

struct FontCacheHeader {
	std::array<char, 8> magic{ kExpectedFontCacheMagic };
	std::uint32_t version{ kExpectedFontCacheVersion };
	std::uint32_t glyph_count{ 0 };
	std::uint32_t kerning_count{ 0 };
};

constexpr std::array<std::byte, 8> kPngSignature{
	std::byte{ 0x89 }, std::byte{ 'P' },  std::byte{ 'N' },	 std::byte{ 'G' },
	std::byte{ '\r' }, std::byte{ '\n' }, std::byte{ 0x1A }, std::byte{ '\n' },
};
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

class MemoryReader {
public:
	explicit MemoryReader(FontBinary binary) : data_{ std::as_bytes(binary.buffer) } {}

	template <BinarySerializable T>
	bool Read(T& value) {
		auto source{ ReadBytes(sizeof(T)) };

		if (!source.has_value()) {
			return false;
		}

		std::memcpy(&value, source->data(), sizeof(T));
		return true;
	}

	bool ReadString(std::string& string) {
		auto initial_offset{ offset_ };

		std::uint64_t serialized_size{ 0 };

		if (!Read(serialized_size)) {
			return false;
		}

		if (serialized_size > std::numeric_limits<std::size_t>::max()) {
			offset_ = initial_offset;
			return false;
		}

		auto byte_count{ static_cast<std::size_t>(serialized_size) }; // NOSONAR
		auto source{ ReadBytes(byte_count) };

		if (!source.has_value()) {
			offset_ = initial_offset;
			return false;
		}

		string.resize(byte_count);

		if (!source->empty()) {
			std::memcpy(string.data(), source->data(), source->size());
		}

		return true;
	}

private:
	[[nodiscard]] std::optional<std::span<const std::byte>> ReadBytes(std::size_t byte_count) {
		if (!CanRead(byte_count)) {
			return std::nullopt;
		}

		auto result{ data_.subspan(offset_, byte_count) };
		offset_ += byte_count;

		return result;
	}

	[[nodiscard]] bool CanRead(std::size_t byte_count) const {
		return offset_ <= data_.size() && byte_count <= data_.size() - offset_;
	}

	std::span<const std::byte> data_;
	std::size_t offset_{ 0 };
};

class MemoryWriter {
public:
	template <BinarySerializable T>
	void Write(const T& value) {
		bytes.append_range(std::as_bytes(std::span{ &value, 1 }));
	}

	void WriteString(std::string_view string) {
		auto size{ static_cast<std::uint64_t>(string.size()) }; // NOSONAR
		Write(size);

		bytes.append_range(std::as_bytes(std::span{ string.data(), string.size() }));
	}

	std::vector<std::byte> bytes;
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

std::uint32_t Crc32(std::span<const std::byte> bytes) {
	std::uint32_t crc{ 0xFFFFFFFFU };

	for (auto byte : bytes) {
		crc ^= std::to_underlying(byte);

		for (int bit{ 0 }; bit < 8; ++bit) {
			crc = (crc & 1U) ? (crc >> 1U) ^ 0xEDB88320U : crc >> 1U;
		}
	}

	return crc ^ 0xFFFFFFFFU;
}

bool HasPngSignature(std::span<const std::byte> bytes) {
	return bytes.size() >= kPngSignature.size() &&
		   std::ranges::equal(kPngSignature, bytes.first(kPngSignature.size()));
}

std::uint32_t ReadBigEndianU32(std::span<const std::byte> bytes, std::size_t offset) {
	PTGN_ASSERT(
		offset <= bytes.size() && bytes.size() - offset >= sizeof(std::uint32_t),
		"Cannot read big-endian uint32_t beyond byte span"
	);

	auto data{ bytes.subspan(offset, sizeof(std::uint32_t)) };

	return (std::to_integer<std::uint32_t>(data[0]) << 24U) |
		   (std::to_integer<std::uint32_t>(data[1]) << 16U) |
		   (std::to_integer<std::uint32_t>(data[2]) << 8U) |
		   std::to_integer<std::uint32_t>(data[3]);
}

void AppendBigEndianU32(std::vector<std::byte>& bytes, std::uint32_t value) {
	bytes.emplace_back(static_cast<std::byte>(value >> 24U));
	bytes.emplace_back(static_cast<std::byte>(value >> 16U));
	bytes.emplace_back(static_cast<std::byte>(value >> 8U));
	bytes.emplace_back(static_cast<std::byte>(value));
}

bool MatchesType(std::span<const std::byte> bytes, std::size_t offset, std::array<char, 4> type) {
	for (std::size_t i{ 0 }; i < type.size(); ++i) {
		if (bytes[offset + i] != static_cast<std::byte>(type[i])) {
			return false;
		}
	}

	return true;
}

bool HasValidPngHeaderChunk(std::span<const std::byte> png_bytes) {
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

std::expected<FontBinary, FontCacheError> GetExpectedFontDataChunkPayload(
	std::span<const std::byte> png_bytes
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

	if (auto chunk_end{ crc_offset + kPngChunkCrcSize }; chunk_end > png_bytes.size()) {
		return std::unexpected{ FontCacheError::InvalidPng };
	}

	auto stored_crc{ ReadBigEndianU32(png_bytes, crc_offset) };

	std::span<const std::byte> crc_bytes{ png_bytes.data() + kFontDataChunkTypeOffset,
										  kPngChunkTypeSize + static_cast<std::size_t>(length) };

	if (auto computed_crc{ Crc32(crc_bytes) }; stored_crc != computed_crc) {
		return std::unexpected{ FontCacheError::CrcMismatch };
	}

	return FontBinary{ std::span{ png_bytes.data() + data_offset,
								  static_cast<std::size_t>(length) } };
}

std::vector<std::byte> WriteFontCachePayload(const impl::FontData& font) {
	MemoryWriter out;

	FontCacheHeader header{ .glyph_count   = static_cast<std::uint32_t>(font.glyphs.size()),
							.kerning_count = static_cast<std::uint32_t>(font.kerning.size()) };

	out.Write(header);
	WritePath(out, font.path);
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

	if (!ReadPath(in, font.path)) {
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

std::vector<std::byte> MakePngChunk(std::array<char, 4> type, std::span<const std::byte> payload) {
	PTGN_ASSERT(
		payload.size() <= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()),
		"PNG chunk payload is too large"
	);

	std::vector<std::byte> chunk;
	chunk.reserve(12 + payload.size());

	AppendBigEndianU32(chunk, static_cast<std::uint32_t>(payload.size()));

	for (auto c : type) {
		chunk.push_back(static_cast<std::byte>(c));
	}

	chunk.append_range(payload);

	constexpr auto crc_offset{ 4 };
	auto crc{ Crc32(std::as_bytes(std::span{ chunk }.subspan(crc_offset))) };

	AppendBigEndianU32(chunk, crc);

	return chunk;
}

impl::Surface CreateSurfaceFromEncodedPng(FontBinary font_png) {
	return impl::Surface{ std::as_bytes(font_png.buffer), kFontAtlasChannelCount };
}

FontCacheError ToFontCacheError(FileWriteError error) {
	switch (error) {
		case FileWriteError::OpenFailed:  return FontCacheError::CannotOpen;
		case FileWriteError::WriteFailed: return FontCacheError::WriteFailed;
		default:						  PTGN_ERROR("Unknown FileWriteError: ", std::to_underlying(error));
	}
}

std::expected<impl::FontData, FontCacheError> ReadFontCacheFromPng(FontBinary font_png) {
	if (font_png.buffer.empty()) {
		return std::unexpected{ FontCacheError::InvalidPng };
	}

	auto png_bytes{ std::as_bytes(font_png.buffer) };

	auto payload{ GetExpectedFontDataChunkPayload(png_bytes) };
	if (!payload.has_value()) {
		return std::unexpected{ payload.error() };
	}

	return ReadFontCachePayload(payload.value());
}

std::expected<impl::FontData, FontCacheError> ReadFontCacheFromPng(const path& png_path) {
	auto png_bytes{ ReadBinary(png_path) };
	return ReadFontCacheFromPng(FontBinary{ png_bytes });
}

std::expected<std::vector<std::byte>, FontCacheError> InsertPngChunkAfterIhdr(
	std::span<const std::byte> png_bytes, std::array<char, 4> type,
	std::span<const std::byte> payload
) {
	if (!HasValidPngHeaderChunk(png_bytes)) {
		return std::unexpected{ FontCacheError::InvalidPng };
	}

	auto inserted_chunk{ MakePngChunk(type, payload) };

	std::vector<std::byte> output;
	output.reserve(png_bytes.size() + inserted_chunk.size());

	output.append_range(png_bytes.first(kPngSignature.size()));

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

		// Remove older copies of the same custom chunk when regenerating.
		if (auto is_existing_target_chunk{ MatchesType(png_bytes, type_offset, type) };
			!is_existing_target_chunk) {
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

std::expected<FontAtlasData, FontCacheError> LoadFontCache(FontBinary font_png) {
	if (font_png.buffer.empty()) {
		return std::unexpected{ FontCacheError::InvalidPng };
	}

	auto png_bytes{ std::as_bytes(font_png.buffer) };

	auto embedded_payload{ GetExpectedFontDataChunkPayload(png_bytes) };

	if (!embedded_payload.has_value()) {
		return std::unexpected{ embedded_payload.error() };
	}

	auto font_data{ ReadFontCachePayload(embedded_payload.value()) };

	if (!font_data.has_value()) {
		return std::unexpected{ font_data.error() };
	}

	Surface surface{ png_bytes, kFontAtlasChannelCount };

	return FontAtlasData{ .surface = std::move(surface), .font = std::move(font_data.value()) };
}

std::expected<FontAtlasData, FontCacheError> LoadFontCache(const path& cache_png_path) {
	auto png_bytes{ ReadBinary(cache_png_path) };

	return LoadFontCache(FontBinary{ png_bytes });
}

std::expected<void, FontCacheError> WriteFontCache(
	const path& cache_png_path, const FontAtlasData& data
) {
	auto png_bytes{ data.surface.EncodePNG() };
	auto serialized_data{ WriteFontCachePayload(data.font) };

	auto font_png{ InsertPngChunkAfterIhdr(png_bytes, kFontDataChunkType, serialized_data) };

	if (!font_png.has_value()) {
		return std::unexpected{ font_png.error() };
	}

	if (auto cache_write{ WriteBinary(cache_png_path, font_png.value()) };
		!cache_write.has_value()) {
		return std::unexpected{ ToFontCacheError(cache_write.error()) };
	}

	return {};
}

bool IsFontAtlasPng(const path& png_path) {
	if (!HasExtension(png_path, ".png")) {
		return false;
	}

	std::ifstream in{ GetAbsolutePath(png_path), std::ios::binary };
	if (!in) {
		return false;
	}

	std::array<std::byte, kMinimumFontAtlasPngSize> bytes{};
	in.read(
		reinterpret_cast<char*>(bytes.data()), // NOSONAR
		static_cast<std::streamsize>(bytes.size())
	);

	if (in.gcount() != static_cast<std::streamsize>(bytes.size())) {
		return false;
	}

	return HasValidPngHeaderChunk(bytes) &&
		   MatchesType(bytes, kFontDataChunkTypeOffset, kFontDataChunkType);
}

} // namespace ptgn::impl