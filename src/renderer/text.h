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

enum class TextWrapAlignment {
	Left   = 0, // TTF_WRAPPED_ALIGN_LEFT
	Center = 1, // TTF_WRAPPED_ALIGN_CENTER
	Right  = 2	// TTF_WRAPPED_ALIGN_RIGHT
};

/*
// TODO: Fix.
SDL_Surface* GetSurface
	Font& font, FontStyle style, const Color& text_color, FontRenderMode mode,
	const std::string& content, std::int32_t ptsize, const Color& shading_color,
	std::uint32_t wrap_after_pixels, TextWrapAlignment wrap_alignment, std::int32_t line_skip
) :
	Surface{ std::invoke([&]() {
		PTGN_ASSERT(
			font.IsValid(), "Cannot create a surface with an invalid or uninitialized font"
		);

		auto f = &font.Get();

		TTF_SetFontStyle(f, static_cast<int>(style));
		TTF_SetFontWrappedAlign(f, static_cast<int>(wrap_alignment));
		if (line_skip != std::numeric_limits<std::int32_t>::infinity()) {
		// TODO: Re-enable this for Emscripten once it is supported (SDL_ttf 2.24.0).
#ifndef __EMSCRIPTEN__
			TTF_SetFontLineSkip(f, line_skip);
#endif
		}
		if (ptsize != std::numeric_limits<std::int32_t>::infinity()) {
			TTF_SetFontSize(f, ptsize);
		}

		std::shared_ptr<SDL_Surface> surface;

		SDL_Color tc{ text_color.r, text_color.g, text_color.b, text_color.a };

		switch (mode) {
			case FontRenderMode::Solid:
				surface = std::shared_ptr<SDL_Surface>{
					TTF_RenderUTF8_Solid_Wrapped(f, content.c_str(), tc, wrap_after_pixels),
					impl::SDL_SurfaceDeleter{}
				};
				break;
			case FontRenderMode::Shaded: {
				SDL_Color sc{ shading_color.r, shading_color.g, shading_color.b, shading_color.a };
				surface = std::shared_ptr<SDL_Surface>{
					TTF_RenderUTF8_Shaded_Wrapped(f, content.c_str(), tc, sc, wrap_after_pixels),
					impl::SDL_SurfaceDeleter{}
				};
				break;
			}
			case FontRenderMode::Blended:
				surface = std::shared_ptr<SDL_Surface>{
					TTF_RenderUTF8_Blended_Wrapped(f, content.c_str(), tc, wrap_after_pixels),
					impl::SDL_SurfaceDeleter{}
				};
				break;
			default:
				PTGN_ERROR(
					"Unrecognized render mode given when creating surface from font information"
				);
		}

		PTGN_ASSERT(surface != nullptr, "Failed to create surface for given font information");

		return surface;
	}) } {
}
*/

V2_int GetSize(Font font, const std::string& content) {
	PTGN_ASSERT(font.IsValid(), "Cannot get size of uninitialized or invalid font");
	V2_int size;
	TTF_SizeUTF8(&font.Get(), content.c_str(), &size.x, &size.y);
	return size;
}

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
	// Background color.
	Color shading_color_{ color::White };
	// 0 indicates only wrapping on newline characters.
	std::uint32_t wrap_after_pixels_{ 0 };
	// Set the spacing between lines of text. Infinity will use the current font line skip.
	std::int32_t line_skip_{ std::numeric_limits<std::int32_t>::infinity() };
	// Set the point size of text. Infinity will use the current point size of the font.
	std::int32_t point_size_{ std::numeric_limits<std::int32_t>::infinity() };
	TextWrapAlignment wrap_alignment_{ TextWrapAlignment::Center };
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
		const FontOrKey& font = {}
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
	// Set the spacing between lines of text.
	Text& SetLineSkip(std::int32_t pixels);
	// Set point size of text.
	Text& SetSize(std::int32_t point_size);
	Text& SetWrapAlignment(TextWrapAlignment wrap_alignment);
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