#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

#include "protegon/color.h"
#include "protegon/font.h"
#include "protegon/polygon.h"
#include "protegon/texture.h"
#include "protegon/vector2.h"
#include "utility/handle.h"

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
	// 0 indicates only wrapping on newline characters.
	std::uint32_t wrap_after_pixels_{ 0 };
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
		const FontOrKey& font, const std::string_view& content, const Color& text_color,
		FontStyle font_style	   = FontStyle::Normal,
		FontRenderMode render_mode = FontRenderMode::Solid,
		const Color& shading_color = color::White
	);

	void SetFont(const FontOrKey& font);
	void SetContent(const std::string_view& content);
	void SetColor(const Color& text_color);
	void SetFontStyle(FontStyle font_style);
	void SetFontRenderMode(FontRenderMode render_mode);
	void SetShadingColor(const Color& shading_color);
	// text wrapped to multiple lines on line endings and on word boundaries if it extends beyond
	// this pixel value. Setting pixels = 0 (default) will wrap only after newlines.
	void SetWrapAfter(std::uint32_t pixels);

	[[nodiscard]] const Font& GetFont() const;
	[[nodiscard]] std::string_view GetContent() const;
	[[nodiscard]] const Color& GetColor() const;
	[[nodiscard]] FontStyle GetFontStyle() const;
	[[nodiscard]] FontRenderMode GetFontRenderMode() const;
	[[nodiscard]] const Color& GetShadingColor() const;

	void SetVisibility(bool visibility);
	[[nodiscard]] bool GetVisibility() const;

	void Draw(const Rectangle<float>& destination) const;

	[[nodiscard]] static V2_int GetSize(const FontOrKey& font, const std::string& content);

private:
	[[nodiscard]] Texture RecreateTexture();

	[[nodiscard]] static Font GetFont(const FontOrKey& font);
};

} // namespace ptgn