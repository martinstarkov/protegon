#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/math/vector2.h"
#include "core/util/entity_handle.h"

#ifdef __EMSCRIPTEN__
struct _TTF_Font;
using TTF_Font = _TTF_Font;
#else
struct TTF_Font;
#endif

namespace ptgn {

static constexpr std::int32_t default_font_size{ 18 };

namespace impl {

struct TTF_FontDeleter {
	void operator()(TTF_Font* font) const;
};

struct FontSize {
	float point_size{ 0.0f };
};

} // namespace impl

struct FontBinary {
	FontBinary() = default;

	FontBinary(unsigned char* font_buffer, unsigned int buffer_length) :
		buffer{ font_buffer }, length{ buffer_length } {}

	unsigned char* buffer{ nullptr };
	unsigned int length{ 0 };
};

enum class FontRenderMode : int {
	Blended = 0,
	Solid	= 1,
	Shaded	= 2
};

enum class FontStyle : int {
	Normal		  = 0, // TTF_STYLE_NORMAL
	Bold		  = 1, // TTF_STYLE_BOLD
	Italic		  = 2, // TTF_STYLE_ITALIC
	Underline	  = 4, // TTF_STYLE_UNDERLINE
	Strikethrough = 8  // TTF_STYLE_STRIKETHROUGH
};

[[nodiscard]] inline FontStyle operator&(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(std::to_underlying(a) | std::to_underlying(b));
}

[[nodiscard]] inline FontStyle operator|(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(std::to_underlying(a) | std::to_underlying(b));
}

class Font : public EntityHandle {
public:
	using EntityHandle::EntityHandle;

	int GetLineSkip(std::optional<float> font_size) const;

	/// @param Text to calculate size of, in UTF-8 encoding.
	/// @param font_size Optional font size to check the size for. If {}, uses the current font
	/// size.
	/// @param max_wrap_width The maximum width or 0 to wrap on newline characters.
	V2_int GetSize(
		const std::string& content, std::optional<float> font_size = {}, int max_wrap_width = 0
	) const;

	/// @param font_size Optional font size to check the height for. If {}, uses the current font
	/// size.
	int GetHeight(std::optional<float> font_size = {}) const;

private:
	std::shared_ptr<TTF_Font> Get(std::optional<float> font_size) const;

	friend class AssetManager;
};

} // namespace ptgn