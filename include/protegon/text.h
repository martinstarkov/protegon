#pragma once

#include <string>
#include <variant>

#include "color.h"
#include "polygon.h"
#include "texture.h"
#include "font.h"
#include "handle.h"

namespace ptgn {

namespace impl {

struct TextInstance {
	TextInstance() = default;
	TextInstance(const Font& font, const std::string& content, const Color& color);

	Font font_;
	std::string content_;
	Color color_;
	Texture texture_;

	Font::Style style_{ Font::Style::NORMAL };
	Font::RenderMode mode_{ Font::RenderMode::SOLID };
	Color bg_shading_{ color::WHITE };
	bool visible_{ true };
};

} // namespace impl

class Text : public Handle<impl::TextInstance> {
public:
	using FontOrKey = std::variant<std::size_t, Font>;

	Text() = default;
	Text(const FontOrKey& font, const std::string& content, const Color& color);

	void SetVisibility(bool visibility);
	bool GetVisibility() const;
	void SetContent(const std::string& new_content);
	void SetColor(const Color& new_color);
	void SetFont(const FontOrKey& new_font);
	void SetSolidRenderMode();
	void SetShadedRenderMode(const Color& bg_shading);
	void SetBlendedRenderMode();

	// Accepts any number of FontStyle enum values (UNDERLINED, BOLD, etc).
	// These are combined into one style and text is renderer in that style.
	template <typename ...Style,
		type_traits::type<Font::Style, Style...> = true>
	void SetStyles(Style... styles) {
		style_ = (styles | ...);
		Refresh();
	}

	void Draw(const Rectangle<float>& box) const;
private:
	static Font GetFont(const FontOrKey& font);
	// Call Refresh() if you change any attribute of the text.
	void Refresh();
};

} // namespace ptgn