#pragma once

#include <string>
#include <variant>

#include "protegon/color.h"
#include "protegon/font.h"
#include "protegon/polygon.h"
#include "protegon/texture.h"
#include "utility/handle.h"

struct SDL_Texture;

namespace ptgn {

namespace impl {

struct TextInstance {
	TextInstance()	= default;
	~TextInstance() = default;

	Texture texture_;
	Font font_;
	std::string content_;
	Color text_color_;
	FontStyle font_style_{ FontStyle::Normal };
	FontRenderMode render_mode_{ FontRenderMode::Solid };
	Color shading_color_;
	bool visible_{ true };
};

} // namespace impl

class Text : public Handle<impl::TextInstance> {
public:
	using FontOrKey = std::variant<std::size_t, Font>;

	Text() = default;
	// To create text with multiple FontStyles, simply use &&, e.g.
	// FontStyle::Italic && FontStyle::Bold
	Text(
		const FontOrKey& font, const std::string& content, const Color& text_color,
		FontStyle font_style	   = FontStyle::Normal,
		FontRenderMode render_mode = FontRenderMode::Solid,
		const Color& shading_color = color::White
	);

	void SetFont(const FontOrKey& font);
	void SetContent(const std::string& content);
	void SetColor(const Color& text_color);
	void SetFontStyle(FontStyle font_style);
	void SetFontRenderMode(FontRenderMode render_mode);
	void SetShadingColor(const Color& shading_color);

	[[nodiscard]] const Font& GetFont() const;
	[[nodiscard]] const std::string& GetContent() const;
	[[nodiscard]] const Color& GetColor() const;
	[[nodiscard]] FontStyle GetFontStyle() const;
	[[nodiscard]] FontRenderMode GetFontRenderMode() const;
	[[nodiscard]] const Color& GetShadingColor() const;

	void SetVisibility(bool visibility);
	[[nodiscard]] bool GetVisibility() const;

	void Draw(const Rectangle<int>& destination) const;

	[[nodiscard]] static V2_int GetSize(const FontOrKey& font, const std::string& content);

private:
	Texture RecreateTexture();

	[[nodiscard]] static Font GetFont(const FontOrKey& font);
};

} // namespace ptgn