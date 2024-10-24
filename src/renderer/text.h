#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

#include "core/manager.h"
#include "renderer/color.h"
#include "renderer/font.h"
#include "math/geometry/polygon.h"
#include "renderer/texture.h"
#include "math/vector2.h"
#include "utility/handle.h"

namespace ptgn {

namespace impl {

struct TextInstance {
	TextInstance()	= default;
	~TextInstance() = default;

	Texture texture_;
	Font font_;
	std::string content_;
	Color text_color_{ color::Black };
	FontStyle font_style_{ FontStyle::Normal };
	FontRenderMode render_mode_{ FontRenderMode::Solid };
	Color shading_color_{ color::White };
	// 0 indicates only wrapping on newline characters.
	std::uint32_t wrap_after_pixels_{ 0 };
	bool visible_{ true };
};

} // namespace impl

class Text : public Handle<impl::TextInstance> {
public:
	Text() = default;
	// To create text with multiple FontStyles, simply use &&, e.g.
	// FontStyle::Italic && FontStyle::Bold
	// @param font Default: {}, which corresponds to the default font (use game.font.SetDefault(...)
	// to change.
	Text(
		const std::string_view& content, const Color& text_color, const FontOrKey& font = {},
		FontStyle font_style	   = FontStyle::Normal,
		FontRenderMode render_mode = FontRenderMode::Solid,
		const Color& shading_color = color::White, std::uint32_t wrap_after_pixels = 0
	);

	Text& SetFont(const FontOrKey& font);
	Text& SetContent(const std::string_view& content);
	Text& SetColor(const Color& text_color);
	Text& SetFontStyle(FontStyle font_style);
	Text& SetFontRenderMode(FontRenderMode render_mode);
	Text& SetShadingColor(const Color& shading_color);
	// text wrapped to multiple lines on line endings and on word boundaries if it extends beyond
	// this pixel value. Setting pixels = 0 (default) will wrap only after newlines.
	Text& SetWrapAfter(std::uint32_t pixels);
	Text& SetVisibility(bool visibility);
	Text& ToggleVisibility();

	// TODO: Add https://wiki.libsdl.org/SDL2_ttf/TTF_SetFontWrappedAlign to set wrap alignment.

	[[nodiscard]] const Font& GetFont() const;
	[[nodiscard]] std::string_view GetContent() const;
	[[nodiscard]] const Color& GetColor() const;
	[[nodiscard]] FontStyle GetFontStyle() const;
	[[nodiscard]] FontRenderMode GetFontRenderMode() const;
	[[nodiscard]] const Color& GetShadingColor() const;
	[[nodiscard]] const Texture& GetTexture() const;
	[[nodiscard]] bool GetVisibility() const;

	[[nodiscard]] V2_int GetSize() const;

	[[nodiscard]] static V2_int GetSize(const FontOrKey& font, const std::string& content);

private:
	[[nodiscard]] Texture RecreateTexture();
};

namespace impl {

class TextManager : public MapManager<Text> {
public:
	using MapManager::MapManager;
};

} // namespace impl

} // namespace ptgn