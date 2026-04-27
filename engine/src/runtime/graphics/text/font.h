#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "serialization/serialize.h"

namespace ptgn {

inline constexpr float kDefaultFontSize{ 18.0f };

/// @brief Defaults to default engine font size.
struct FontSize {
	FontSize() = default;

	FontSize(float value) : value{ value } {}

	float value{ kDefaultFontSize };

	operator float() const { // NOSONAR
		return value;
	}

	PTGN_SERIALIZE_VALUE(FontSize, value)
};

struct FontBinary {
	FontBinary() = default;

	FontBinary(unsigned char* font_buffer, unsigned int buffer_length) :
		buffer{ font_buffer }, length{ buffer_length } {}

	unsigned char* buffer{ nullptr };
	unsigned int length{ 0 };
};

namespace impl {

class Renderer;

struct FontAtlasInfo {
	constexpr FontAtlasInfo() = default;

	float em_size{ 48.0f };
	float pixel_range{ 4.0f };
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
};

struct FontData {
	path font_path;
	FontMetrics metrics;
	std::unordered_map<std::uint32_t, GlyphMetrics> glyphs;
	std::unordered_map<std::uint64_t, float> kerning;
};

class FontObject {
public:
	FontObject() = default;

	FontObject(
		Renderer& renderer, path font_path, path cache_directory, std::string_view cache_name,
		const FontAtlasInfo& atlas_info = {}
	);

#ifndef __EMSCRIPTEN__
	FontObject(Renderer& renderer, path cache_directory, std::string_view cache_name);
#endif

	std::optional<GlyphMetrics> GetGlyph(std::uint32_t codepoint) const;

	float GetAdvance(std::uint32_t current_codepoint, std::uint32_t next_codepoint) const;

	const FontData& GetFontData() const;

	TextureId GetAtlasTexture() const;

	V2_int GetAtlasSize() const;

private:
	TextureObject atlas_texture_;

	FontData data_;
};

} // namespace impl

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