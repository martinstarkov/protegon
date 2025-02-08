#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "components/generic.h"
#include "ecs/ecs.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/font.h"
#include "renderer/texture.h"
#include "utility/type_traits.h"

namespace ptgn {

enum class TextWrapAlignment {
	Left   = 0, // TTF_WRAPPED_ALIGN_LEFT
	Center = 1, // TTF_WRAPPED_ALIGN_CENTER
	Right  = 2	// TTF_WRAPPED_ALIGN_RIGHT
};

struct TextContent : public StringViewComponent {
	using StringViewComponent::StringViewComponent;
};

struct FontKey : public ArithmeticComponent<std::size_t> {
	using ArithmeticComponent::ArithmeticComponent;

	FontKey() : ArithmeticComponent{ Hash("") } {}
};

struct FontSize : public ArithmeticComponent<std::int32_t> {
	using ArithmeticComponent::ArithmeticComponent;

	FontSize() : ArithmeticComponent{ std::numeric_limits<std::int32_t>::infinity() } {}
};

struct TextLineSkip : public ArithmeticComponent<std::int32_t> {
	using ArithmeticComponent::ArithmeticComponent;

	TextLineSkip() : ArithmeticComponent{ std::numeric_limits<std::int32_t>::infinity() } {}
};

struct TextWrapAfter : public ArithmeticComponent<std::uint32_t> {
	using ArithmeticComponent::ArithmeticComponent;
};

struct TextColor : public ColorComponent {
	using ColorComponent::ColorComponent;

	TextColor() : ColorComponent{ color::Black } {}
};

struct TextShadingColor : public ColorComponent {
	using ColorComponent::ColorComponent;

	TextShadingColor() : ColorComponent{ color::White } {}
};

class Text {
public:
	Text() = default;

	// @param font_key Default: "" corresponds to the default engine font (use
	// game.font.SetDefault(...) to change.
	Text(
		std::string_view content, const Color& text_color = color::Black,
		std::string_view font_key = ""
	);
	Text(const Text&)			 = delete;
	Text& operator=(const Text&) = delete;
	Text(Text&& other) noexcept;
	Text& operator=(Text&& other) noexcept;
	~Text();

	// @param font_key Default: "" corresponds to the default engine font (use
	// game.font.SetDefault(...) to change.
	Text& SetFont(std::string_view font_key = "");
	Text& SetContent(std::string_view content);
	Text& SetColor(const Color& color);

	// To create text with multiple FontStyles, simply use &&, e.g.
	// FontStyle::Italic && FontStyle::Bold
	Text& SetFontStyle(FontStyle font_style);
	// Set the point size of text. Infinity will use the current point size of the font.
	Text& SetFontSize(std::int32_t pixels);

	Text& SetFontRenderMode(FontRenderMode render_mode);

	// Sets the background shading color for the text.
	// Also sets the font render mode to FontRenderMode::Shaded.
	Text& SetShadingColor(const Color& shading_color);

	// text wrapped to multiple lines on line endings and on word boundaries if it extends beyond
	// this pixel value. Setting pixels = 0 (default) will wrap only after newlines.
	Text& SetWrapAfter(std::uint32_t pixels);
	// Set the spacing between lines of text. Infinity will use the current font line skip.
	Text& SetLineSkip(std::int32_t pixels);

	Text& SetWrapAlignment(TextWrapAlignment wrap_alignment);

	[[nodiscard]] std::size_t GetFontKey() const;
	[[nodiscard]] std::string_view GetContent() const;
	[[nodiscard]] Color GetColor() const;
	[[nodiscard]] FontStyle GetFontStyle() const;
	[[nodiscard]] FontRenderMode GetFontRenderMode() const;
	[[nodiscard]] Color GetShadingColor() const;
	[[nodiscard]] const impl::Texture& GetTexture() const;

	[[nodiscard]] std::int32_t GetFontSize() const;

	// @return The unscaled size of the text texture given the current content and font.
	[[nodiscard]] V2_int GetSize() const;

private:
	template <typename T>
	Text& SetParameter(const T& value) {
		static_assert(tt::is_any_of_v<
					  T, FontKey, TextContent, TextColor, FontStyle, FontRenderMode, FontSize,
					  TextLineSkip, TextShadingColor, TextWrapAfter, TextWrapAlignment>);
		if (!entity_.Has<T>()) {
			entity_.Add<T>(value);
			RecreateTexture();
			return *this;
		}
		auto& t{ entity_.Get<T>() };
		if (t == value) {
			return *this;
		}
		t = value;
		RecreateTexture();
		return *this;
	}

	template <typename T>
	[[nodiscard]] const T& GetParameter(const T& default_value) const {
		static_assert(tt::is_any_of_v<
					  T, FontKey, TextContent, TextColor, FontStyle, FontRenderMode, FontSize,
					  TextLineSkip, TextShadingColor, TextWrapAfter, TextWrapAlignment>);
		if (!entity_.Has<T>()) {
			return default_value;
		}
		return entity_.Get<T>();
	}

	void RecreateTexture();

	ecs::Entity entity_;
};

} // namespace ptgn