#pragma once

#include <string>

#include "color.h"
#include "rectangle.h"
#include "texture.h"
#include "font.h"

namespace ptgn {

class Text {
public:
	Text(std::size_t font_key, const std::string& content, const Color& color);
	Text(const Font& font, const std::string& content, const Color& color);
	~Text() = default;
	Text(const Text&) = default;
	Text& operator=(const Text&) = default;
	Text(Text&&) = default;
	Text& operator=(Text&&) = default;

	void SetVisibility(bool visibility);
	bool GetVisibility() const;
	void SetContent(const std::string& new_content);
	void SetColor(const Color& new_color);
	void SetFont(const Font& new_font);
	void SetSolidRenderMode();
	void SetShadedRenderMode(const Color& bg_shading);
	void SetBlendedRenderMode();
	bool IsValid() const;
	// Accepts any number of FontStyle enum values (UNDERLINED, BOLD, etc).
	// These are combined into one style and text is renderer in that style.
	template <typename ...Style,
		type_traits::type<Font::Style, Style...> = true>
	void SetStyles(Style... styles) {
		style_ = (styles | ...);
		Refresh();
	}
	void Draw(Rectangle<float> box) const;
private:
	Text() = default;
	// TLDR; Call this if you change any attribute of the text.
	void Refresh();
	Texture texture_;
	Font font_;
	std::string content_;
	Color color_{};
	Font::Style style_{ Font::Style::NORMAL };
	Font::RenderMode mode_{ Font::RenderMode::SOLID };
	Color bg_shading_{ color::WHITE };
	bool visible_{ true };
};

} // namespace ptgn