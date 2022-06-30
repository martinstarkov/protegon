#pragma once

#include "text/FontRenderMode.h"
#include "text/FontStyle.h"
#include "renderer/Colors.h"
#include "managers/Manager.h"
#include "renderer/Renderer.h"
#include "texture/Texture.h"

namespace ptgn {

namespace internal {

struct Text {
public:
	Text() = delete;
	Text(const Renderer& renderer, const managers::id font_key, const char* content, const Color& color);
	void SetContent(const char* new_content);
	void SetColor(const Color& new_color);
	void SetFont(const managers::id new_font_key);
	void SetSolidRenderMode();
	void SetShadedRenderMode(const Color& background_shading);
	void SetBlendedRenderMode();
	// Accepts any number of FontStyle enum values (UNDERLINED, BOLD, etc).
	// These are combined into one style and text is renderer in that style.
	template <typename ...Style,
		type_traits::are_type_e<FontStyle, Style...> = true>
		void SetStyles(Style... styles) {
		style_ = (static_cast<int>(styles) | ...);
		Refresh();
	}
	void Draw(const V2_int& text_position, const V2_int& text_size) const;
private:
	void Refresh();
	Texture texture_;
	managers::id font_key_;
	const char* content_;
	Color color_;
	int style_{ static_cast<int>(FontStyle::NORMAL) };
	Color background_shading_{ color::WHITE };
	FontRenderMode mode_{ FontRenderMode::SOLID };
};

} // namespace internal

} // namespace ptgn