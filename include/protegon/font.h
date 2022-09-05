#pragma once

#include <cstdint> // std::int32_t, std::uint32_t
#include <memory>  // std::shared_ptr

struct _TTF_Font;
using TTF_Font = _TTF_Font;

namespace ptgn {

class Font {
public:
	Font(const char* font_path, std::uint32_t point_size, std::uint32_t index = 0);
	~Font() = default;
	Font(const Font&) = default;
	Font& operator=(const Font&) = default;
	Font(Font&&) = default;
	Font& operator=(Font&&) = default;

	std::int32_t GetHeight() const;
	bool IsValid() const;
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
private:
	Font() = default;
	friend class Text;
	struct TTF_Font_Deleter {
		void operator()(TTF_Font* font);
	};
	std::shared_ptr<TTF_Font> font_{ nullptr };
};

inline Font::Style operator|(Font::Style a, Font::Style b) {
	return static_cast<Font::Style>(static_cast<int>(a) | static_cast<int>(b));
}

} // namespace ptgn