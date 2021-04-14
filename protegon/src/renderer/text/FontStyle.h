#pragma once

namespace engine {

enum class FontStyle : int {
	NORMAL = 0, // TTF_STYLE_NORMAL
	BOND = 1, // TTF_STYLE_BOLD
	ITALIC = 2, // TTF_STYLE_ITALIC
	UNDERLINE = 4, // TTF_STYLE_UNDERLINE
	STRIKETHROUGH = 8 // TTF_STYLE_STRIKETHROUGH
};

} // namespace engine