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
	TextInstance(const Texture& texture);
	~TextInstance() = default;

	Texture texture_;
	bool visible_{ true };
};

} // namespace impl

class Text : public Handle<impl::TextInstance> {
public:
	using FontOrKey = std::variant<std::size_t, Font>;

	Text() = default;
	// To create text with multiple FontStyles, simply use &&, e.g. FontStyle::Italic && FontStyle::Bold
	Text(
		const FontOrKey& font,
		const std::string& content,
		const Color& text_color,
		FontStyle font_style = FontStyle::Normal,
		FontRenderMode render_mode = FontRenderMode::Solid,
		const Color& shading_color = color::White
	);

	void SetVisibility(bool visibility);
	[[nodiscard]] bool GetVisibility() const;

	void Draw(const Rectangle<float>& destination) const;
private:
	[[nodiscard]] static Font GetFont(const FontOrKey& font);
	[[nodiscard]] static Texture CreateTexture(
		const Font& font,
		const std::string& content,
		const Color& text_color,
		FontStyle font_style,
		FontRenderMode render_mode,
		const Color& shading_color
	);
};

} // namespace ptgn