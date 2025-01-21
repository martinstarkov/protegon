#pragma once

#include <cstdint>
#include <string>

#include "core/manager.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/font.h"
#include "renderer/texture.h"
#include "utility/handle.h"

namespace ptgn {

struct Rect;

struct TextInfo {
	TextInfo(
		FontStyle font_style	   = FontStyle::Normal,
		FontRenderMode render_mode = FontRenderMode::Solid,
		const Color& shading_color = color::White, std::uint32_t wrap_after_pixels = 0,
		bool visible = true
	) :
		font_style{ font_style },
		render_mode{ render_mode },
		shading_color{ shading_color },
		wrap_after_pixels{ wrap_after_pixels },
		visible{ visible } {}

	FontStyle font_style{ FontStyle::Normal };
	FontRenderMode render_mode{ FontRenderMode::Solid };
	Color shading_color{ color::White };
	// 0 indicates only wrapping on newline characters.
	std::uint32_t wrap_after_pixels{ 0 };
	bool visible{ true };
};

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
		const std::string_view& content, const Color& text_color = color::Black,
		const FontOrKey& font = {}, FontStyle font_style = FontStyle::Normal,
		FontRenderMode render_mode = FontRenderMode::Solid,
		const Color& shading_color = color::White, std::uint32_t wrap_after_pixels = 0
	);

	// Setting destination.size == {} corresponds to the unscaled size of the text.
	void Draw(const Rect& destination, std::int32_t render_layer = 0) const;

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
	TextManager()								   = default;
	~TextManager() override						   = default;
	TextManager(TextManager&&) noexcept			   = default;
	TextManager& operator=(TextManager&&) noexcept = default;
	TextManager(const TextManager&)				   = delete;
	TextManager& operator=(const TextManager&)	   = delete;
};

} // namespace impl

} // namespace ptgn