#pragma once

#include <string>

#include "core/Engine.h"

#include "math/Vector2.h"

#include "utils/TypeTraits.h"

#include "renderer/Color.h"
#include "renderer/Texture.h"
#include "renderer/Renderer.h"
#include "renderer/text/Font.h"
#include "renderer/text/FontStyle.h"

namespace engine {

// Text must be freed using Destroy().
class Text {
public:
	Text() = default;
	Text(const char* content,
		 const Color& color,
		 const char* font_name,
		 const V2_double& position,
		 const V2_double& area,
		 std::size_t display_index = 0);
	Text& operator=(Text&& obj) noexcept;
	// TODO: Add ability to copy text.

	void Destroy();

	void SetDisplay(const Window& window);
	void SetDisplay(const Renderer& renderer);
	void SetDisplay(std::size_t display_index = 0);

	void SetContent(const char* new_content);
	void SetColor(const Color& new_color);
	void SetFont(const char* new_font_name);
	void SetPosition(const V2_double& new_position);
	void SetArea(const V2_double& new_area);

	// Function accepts any number of FontStyle enum values (UNDERLINED, BOLD, etc).
	// These are combined into one style and text is renderer in that style.
	template <typename ...Style,
		type_traits::are_type<FontStyle, Style...> = true>
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
	Texture GetTexture() const;
	V2_double GetPosition() const;
	V2_double GetArea() const;
private:

	void RefreshTexture();

	int style_{ static_cast<int>(FontStyle::NORMAL) };
	RenderMode mode_{ RenderMode::SOLID };
	Color shading_background_color_;
	Texture texture_;

	const char* content_{ "" };
	Color color_;
	const char* font_name_{ "" };
	std::size_t font_key_{ 0 };
	V2_double position_;
	V2_double area_;
	std::size_t display_index_{ 0 };
};

} // namespace engine