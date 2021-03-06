#pragma once

#include "renderer/Texture.h"
#include "renderer/Color.h"
#include "renderer/text/FontRenderMode.h"
#include "renderer/text/FontStyle.h"
#include "utils/TypeTraits.h"

namespace ptgn {

class Text {
public:
	Text() = default;
	~Text();
	// Construct text.
	Text(const char* content,
		 const Color& color,
		 const char* font_name);
	Text& operator=(Text&& obj) noexcept;

	// Set text content.
	void SetContent(const char* new_content);

	// Set text color.
	void SetColor(const Color& new_color);

	// Set text font to a font that has been loaded into FontManager.
	void SetFont(const char* new_font_name);

	// Accepts any number of FontStyle enum values (UNDERLINED, BOLD, etc).
	// These are combined into one style and text is renderer in that style.
	template <typename ...Style,
		type_traits::are_type_e<FontStyle, Style...> = true>
	void SetStyles(Style... styles) {
		style_ = (static_cast<int>(styles) | ...);
		RefreshTexture();
	}

	void SetSolidRenderMode();

	void SetShadedRenderMode(const Color& shading_background_color);

	void SetBlendedRenderMode();
	
	const char* GetContent() const;

	const char* GetFont() const;

	Color GetColor() const;
private:
	friend class ScreenRenderer;

	Texture GetTexture() const;

	void RefreshTexture();

	int style_{ static_cast<int>(FontStyle::NORMAL) };
	FontRenderMode mode_{ FontRenderMode::SOLID };
	Color shading_background_color_;
	Texture texture_;

	const char* content_{ "" };
	Color color_;
	const char* font_name_{ "" };
	std::size_t font_key_{ 0 };
};

} // namespace ptgn