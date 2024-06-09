#pragma once

#include <cstdint>

#include "handle.h"
#include "file.h"

struct _TTF_Font;
using TTF_Font = _TTF_Font;

namespace ptgn {

enum class FontStyle : int {
	Normal        = 0, // TTF_STYLE_NORMAL
	Bold		  = 1, // TTF_STYLE_BOLD
	Italic        = 2, // TTF_STYLE_ITALIC
	Underline     = 4, // TTF_STYLE_UNDERLINE
	Strikethrough = 8  // TTF_STYLE_STRIKETHROUGH
};

enum class FontRenderMode : int {
	Solid   = 0,
	Shaded  = 1,
	Blended = 2
};

class Font : public Handle<TTF_Font> {
public:
	Font() = default;
	Font(const path& font_path, std::uint32_t point_size, std::uint32_t index = 0);

	[[nodiscard]] std::int32_t GetHeight() const;
};

inline FontStyle operator&(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
}

inline FontStyle operator&&(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
}

inline FontStyle operator|(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
}

} // namespace ptgn