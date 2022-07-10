#pragma once

#include <cstdlib> // std::size_t

#include "text/FontRenderMode.h"
#include "text/FontStyle.h"
#include "renderer/Colors.h"

namespace ptgn {

namespace type_traits {

// Template qualifier of whether or not all Types are same as a given Type.
template <typename Type, typename ...Types>
using are_type_e = std::enable_if_t<std::conjunction_v<std::is_same<Type, Types>...>, bool>;

} // namespace type_traits

class Text {
public:
	Text() = delete;
	Text(const std::size_t texture_key, const std::size_t font_key, const char* content, const Color& color);
	~Text();
	void SetContent(const char* new_content) {
		content_ = new_content;
		Refresh();
	}
	void SetColor(const Color& new_color) {
		color_ = new_color;
		Refresh();
	}
	void SetFont(const std::size_t new_font_key);
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
		type_traits::are_type_e<FontStyle, Style...> = true>
	void SetStyles(Style... styles) {
		style_ = (static_cast<int>(styles) | ...);
		Refresh();
	}
private:
	void Refresh();
	std::size_t texture_key_;
	std::size_t font_key_;
	const char* content_;
	Color color_;
	int style_{ static_cast<int>(FontStyle::NORMAL) };
	Color background_shading_{ color::WHITE };
	FontRenderMode mode_{ FontRenderMode::SOLID };
};

} // namespace ptgn