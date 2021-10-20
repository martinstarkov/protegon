#pragma once

#include "text/FontRenderMode.h"
#include "text/FontStyle.h"
#include "renderer/Colors.h"
#include "utils/TypeTraits.h"

class SDL_Texture;

namespace ptgn {

namespace impl {

class SDLText {
public:
	SDLText() = delete;
	~SDLText();
	SDLText(const char* content, const char* font_key, const Color& color = colors::BLACK);

	// Set text content.
	void SetContent(const char* new_content);

	// Set text color.
	void SetColor(const Color& new_color);

	// Set text font to a font that has been loaded into FontManager.
	void SetFont(const char* new_font_key);

	// Accepts any number of FontStyle enum values (UNDERLINED, BOLD, etc).
	// These are combined into one style and text is renderer in that style.
	template <typename ...Style,
		type_traits::are_type_e<FontStyle, Style...> = true>
	void SetStyles(Style... styles) {
		style_ = (static_cast<int>(styles) | ...);
		RefreshTexture();
	}

	void SetSolidRenderMode();

	void SetShadedRenderMode(const Color& background_shading);

	void SetBlendedRenderMode();
private:
	void RefreshTexture();

	std::size_t font_key_;
	std::size_t texture_key_;
	int style_{ static_cast<int>(FontStyle::NORMAL) };
	const char* content_{ "Default SDL Text" };
	Color color_{ colors::BLACK };
	Color background_shading_{ colors::WHITE };
	FontRenderMode mode_{ FontRenderMode::SOLID };
};

} // namespace impl

} // namespace ptgn