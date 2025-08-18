#pragma once

#include <cstdint>
#include <limits>
#include <string>

#include "common/type_traits.h"
#include "components/drawable.h"
#include "components/generic.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/font.h"
#include "renderer/texture.h"
#include "serialization/enum.h"
#include "serialization/serializable.h"

namespace ptgn {

class Scene;

namespace impl {

class RenderData;
struct ButtonText;

struct HDText : public BoolComponent {
	using BoolComponent::BoolComponent;

	HDText() : BoolComponent{ true } {}
};

struct CachedFontSize : public FontSize {
	using FontSize::FontSize;
};

} // namespace impl

enum class TextJustify {
	Left   = 0, // TTF_WRAPPED_ALIGN_LEFT
	Center = 1, // TTF_WRAPPED_ALIGN_CENTER
	Right  = 2	// TTF_WRAPPED_ALIGN_RIGHT
};

struct TextContent : public StringComponent {
	using StringComponent::StringComponent;
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

struct TextOutline {
	std::int32_t width{ 0 };
	Color color;

	friend bool operator==(const TextOutline& a, const TextOutline& b) {
		return a.width == b.width && a.color == b.color;
	}

	friend bool operator!=(const TextOutline& a, const TextOutline& b) {
		return !(a == b);
	}

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(TextOutline, width, color)
};

struct TextShadingColor : public ColorComponent {
	using ColorComponent::ColorComponent;

	TextShadingColor() : ColorComponent{ color::White } {}
};

struct TextProperties {
	FontStyle style{};
	TextJustify justify{};
	TextLineSkip line_skip{};
	TextWrapAfter wrap_after{};
	FontRenderMode render_mode{};
	TextOutline outline{};
	TextShadingColor shading_color{};

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		TextProperties, style, justify, line_skip, wrap_after, render_mode, outline, shading_color
	)
};

class Text : public Entity, public Drawable<Text> {
public:
	Text() = default;

	Text(const Entity& entity);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	[[nodiscard]] impl::Texture CreateTexture(const FontSize& font_size) const;

	static [[nodiscard]] impl::Texture CreateTexture(
		const std::string& content, const TextColor& color = color::White,
		const FontSize& font_size = {}, const ResourceHandle& font_key = {},
		const TextProperties& properties = {}
	);

	// Set text to be in high definition instead of natively scaling to its camera.
	Text& SetHD(bool hd = true);
	[[nodiscard]] bool IsHD() const;

	// @param font_key Default: "" corresponds to the default engine font (use
	// game.font.SetDefault(...) to change.
	Text& SetFont(const ResourceHandle& font_key = {});
	Text& SetContent(const TextContent& content);
	Text& SetColor(const TextColor& color);

	// To create text with multiple FontStyles, simply use &&, e.g.
	// FontStyle::Italic && FontStyle::Bold
	Text& SetFontStyle(FontStyle font_style);

	// Set the point size of text. Infinity will use the current point size of the font.
	Text& SetFontSize(const FontSize& pixels);

	// Note: This function will implicitly set font render mode to Blended as it is required.
	// @param outline Setting outline.width to 0 will remove the text outline.
	Text& SetOutline(const TextOutline& outline);

	Text& SetFontRenderMode(FontRenderMode render_mode);

	// Sets the background shading color for the text.
	// Also sets the font render mode to FontRenderMode::Shaded.
	Text& SetShadingColor(const Color& shading_color);

	// text wrapped to multiple lines on line endings and on word boundaries if it extends beyond
	// this pixel value. Setting pixels = 0 (default) will wrap only after newlines.
	Text& SetWrapAfter(const TextWrapAfter& pixels);

	// Set the spacing between lines of text. Infinity will use the current font line skip.
	Text& SetLineSkip(const TextLineSkip& pixels);

	// Determines how text is justified.
	Text& SetTextJustify(TextJustify text_justify);

	[[nodiscard]] ResourceHandle GetFontKey() const;
	[[nodiscard]] TextContent GetContent() const;
	[[nodiscard]] TextColor GetColor() const;
	[[nodiscard]] FontStyle GetFontStyle() const;
	[[nodiscard]] FontRenderMode GetFontRenderMode() const;
	[[nodiscard]] Color GetShadingColor() const;
	[[nodiscard]] TextJustify GetTextJustify() const;
	[[nodiscard]] FontSize GetFontSize() const;

	// If SetHD(true), returns font_size scaled to high definition.
	[[nodiscard]] FontSize GetHDFontSize(const FontSize& font_size) const;

	// If SetHD(true), returns the text's font size scaled to high definition.
	[[nodiscard]] FontSize GetHDFontSize() const;

	// @return The unscaled size of the text texture given the current content and font.
	[[nodiscard]] V2_int GetSize() const;

	[[nodiscard]] static V2_int GetSize(const Entity& text);

	[[nodiscard]] static V2_int GetSize(
		const std::string& content, const ResourceHandle& font_key, const FontSize& font_size = {}
	);

	[[nodiscard]] TextProperties GetProperties() const;

	void SetProperties(const TextProperties& properties);

private:
	friend Text
	CreateText(Scene&, const TextContent&, const TextColor&, const FontSize&, const ResourceHandle&, const TextProperties&);
	friend struct impl::ButtonText;

	void SetProperties(const TextProperties& properties, bool recreate_texture);

	// Using own properties.
	void RecreateTexture();

	// Using custom properties.
	void RecreateTexture(
		const std::string& content, const TextColor& color, const FontSize& font_size,
		const ResourceHandle& font_key, const TextProperties& properties
	);

	// @return True if the parameter was changed.
	template <
		typename T,
		tt::enable<tt::is_any_of_v<
			T, ResourceHandle, TextContent, TextColor, FontStyle, FontRenderMode, FontSize,
			TextLineSkip, TextShadingColor, TextWrapAfter, TextOutline, TextJustify>> = true>
	bool SetParameter(const T& value, bool recreate_texture = true) {
		if (!Has<T>()) {
			Add<T>(value);
			if (recreate_texture) {
				RecreateTexture();
			}
			return true;
		}
		T& t{ Get<T>() };
		if (t == value) {
			return false;
		}
		t = value;
		if (recreate_texture) {
			RecreateTexture();
		}
		return true;
	}

	template <typename T>
	[[nodiscard]] const T& GetParameter(const T& default_value) const {
		return GetParameter<T>(*this, default_value);
	}

	template <typename T>
	[[nodiscard]] static const T& GetParameter(const Entity& text, const T& default_value) {
		static_assert(tt::is_any_of_v<
					  T, ResourceHandle, TextContent, TextColor, FontStyle, FontRenderMode,
					  FontSize, TextLineSkip, TextShadingColor, TextWrapAfter, TextOutline,
					  TextJustify>);
		if (!text.Has<T>()) {
			return default_value;
		}
		return text.Get<T>();
	}

private:
	[[nodiscard]] const impl::Texture& GetTexture() const;
};

// @param font_key Default: {} corresponds to the default engine font (use
// game.font.SetDefault(...) to change.
Text CreateText(
	Scene& scene, const TextContent& content, const TextColor& text_color = {},
	const FontSize& font_size = {}, const ResourceHandle& font_key = {},
	const TextProperties& properties = {}
);

// TODO: Implement.
// Create standard definition text.
// @param font_key Default: {} corresponds to the default engine font (use
// game.font.SetDefault(...) to change.
// Text CreateSDText(
//	Scene& scene, const TextContent& content, const TextColor& text_color = {},
//	const FontSize& font_size = {}, const ResourceHandle& font_key = {},
//	const TextProperties& properties = {}
//);

PTGN_SERIALIZER_REGISTER_ENUM(
	TextJustify, { { TextJustify::Left, "left" },
				   { TextJustify::Center, "center" },
				   { TextJustify::Right, "right" } }
);

} // namespace ptgn