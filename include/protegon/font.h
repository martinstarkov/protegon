#pragma once

#include <cstdint>

#include "handle.h"
#include "file.h"

struct _TTF_Font;
using TTF_Font = _TTF_Font;

namespace ptgn {

class Font : public Handle<TTF_Font> {
public:
	enum class Style : int {
		NORMAL = 0,        // TTF_STYLE_NORMAL = 0
		BOLD = 1,          // TTF_STYLE_BOLD = 1
		ITALIC = 2,        // TTF_STYLE_ITALIC = 2
		UNDERLINE = 4,     // TTF_STYLE_UNDERLINE = 4
		STRIKETHROUGH = 8  // TTF_STYLE_STRIKETHROUGH = 8
	};

	enum class RenderMode : int {
		SOLID = 0,
		SHADED = 1,
		BLENDED = 2
	};

	Font() = default;
	Font(const path& font_path, std::uint32_t point_size, std::uint32_t index = 0);

	[[nodiscard]] std::int32_t GetHeight() const;
private:
	friend class Text;
};

inline Font::Style operator|(Font::Style a, Font::Style b) {
	return static_cast<Font::Style>(static_cast<int>(a) | static_cast<int>(b));
}

} // namespace ptgn