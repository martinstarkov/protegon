#pragma once

namespace ptgn {

enum class FontStyle : int {
	NORMAL = 0,        // TTF_STYLE_NORMAL = 0
	BOLD = 1,          // TTF_STYLE_BOLD = 1
	ITALIC = 2,        // TTF_STYLE_ITALIC = 2
	UNDERLINE = 4,     // TTF_STYLE_UNDERLINE = 4
	STRIKETHROUGH = 8  // TTF_STYLE_STRIKETHROUGH = 8
};

inline FontStyle operator|(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
}

} // namespace ptgn