#pragma once

#include "color.h"
#include "rectangle.h"
#include "texture.h" // TODO: temp?

namespace ptgn {

class Text {
public:
	Text() = default;
	Text(const char* font_key, const char* text_content, const Color& text_color);
	~Text();
	void SetContent(const char* new_content) {
		content_ = new_content;
		Refresh();
	}
	void SetColor(const Color& new_color) {
		color_ = new_color;
		Refresh();
	}
	void SetFont(const char* new_font_key);
	void SetSolidRenderMode() {
		mode_ = FontRenderMode::SOLID;
		Refresh();
	}
	void SetShadedRenderMode(const Color& background_shading) {
		background_shading_ = background_shading;
		mode_ = FontRenderMode::SHADED;
		Refresh();
	}
	void SetBlendedRenderMode() {
		mode_ = FontRenderMode::BLENDED;
		Refresh();
	}
	// Accepts any number of FontStyle enum values (UNDERLINED, BOLD, etc).
	// These are combined into one style and text is renderer in that style.
	template <typename ...Style,
		type_traits::type<FontStyle, Style...> = true>
	void SetStyles(Style... styles) {
		style_ = (styles | ...);
		Refresh();
	}
	const std::size_t GetTextureKey() const {
		return texture_key_;
	}
	const std::size_t GetFontKey() const {
		return font_key_;
	}
	void Draw(const Rectangle<int>& box) const;
private:
	void Refresh();
	// Make Text a friend of Texture and make Texture default constructor private?
	Texture texture_;
	std::size_t texture_key_{};
	std::size_t font_key_{};
	const char* content_{};
	Color color_{};
	FontStyle style_{ FontStyle::NORMAL };
	Color background_shading_{ color::WHITE };
	FontRenderMode mode_{ FontRenderMode::SOLID };
};

} // namespace ptgn