#pragma once

#include <cstdint>
#include <limits>

#include "common/type_traits.h"
#include "components/drawable.h"
#include "components/generic.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/resources/font.h"
#include "rendering/resources/texture.h"
#include "serialization/enum.h"
#include "serialization/serializable.h"

namespace ptgn {

class Scene;

namespace impl {

class RenderData;

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
	FontStyle style;
	TextJustify justify;
	TextLineSkip line_skip;
	TextWrapAfter wrap_after;
	FontRenderMode render_mode{ FontRenderMode::Solid };
	TextOutline outline;
	TextShadingColor shading_color;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		TextProperties, style, justify, line_skip, wrap_after, render_mode, outline, shading_color
	)
};

class Text : public Entity, public Drawable<Text> {
public:
	Text() = default;

	Text(const Entity& entity);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	[[nodiscard]] static impl::Texture CreateTexture(
		const std::string& content, const TextColor& color = color::White,
		const FontSize& font_size = {}, const ResourceHandle& font_key = {},
		const TextProperties& properties = {}
	);

	// @param font_key Default: "" corresponds to the default engine font (use
	// game.font.SetDefault(...) to change.
	Text& SetFont(const ResourceHandle& font_key = {});
	Text& SetContent(const TextContent& content);
	Text& SetColor(const TextColor& color);

	// To create text with multiple FontStyles, simply use &&, e.g.
	// FontStyle::Italic && FontStyle::Bold
	Text& SetFontStyle(FontStyle font_style);
	// Set the point size of text. Infinity will use the current point size of the font.
	Text& SetFontSize(std::int32_t pixels);

	// Note: This function will implicitly set font render mode to Blended as it is required.
	// @param width Setting width to 0 will remove the text outline.
	Text& SetOutline(std::int32_t width, const Color& color);

	Text& SetFontRenderMode(FontRenderMode render_mode);

	// Sets the background shading color for the text.
	// Also sets the font render mode to FontRenderMode::Shaded.
	Text& SetShadingColor(const Color& shading_color);

	// text wrapped to multiple lines on line endings and on word boundaries if it extends beyond
	// this pixel value. Setting pixels = 0 (default) will wrap only after newlines.
	Text& SetWrapAfter(std::uint32_t pixels);

	// Set the spacing between lines of text. Infinity will use the current font line skip.
	Text& SetLineSkip(std::int32_t pixels);

	// Determines how text is justified.
	Text& SetTextJustify(TextJustify text_justify);

	[[nodiscard]] ResourceHandle GetFontKey() const;
	[[nodiscard]] TextContent GetContent() const;
	[[nodiscard]] TextColor GetColor() const;
	[[nodiscard]] FontStyle GetFontStyle() const;
	[[nodiscard]] FontRenderMode GetFontRenderMode() const;
	[[nodiscard]] Color GetShadingColor() const;
	[[nodiscard]] TextJustify GetTextJustify() const;
	[[nodiscard]] const impl::Texture& GetTexture() const;

	[[nodiscard]] std::int32_t GetFontSize() const;

	// @return The unscaled size of the text texture given the current content and font.
	[[nodiscard]] static V2_int GetSize(const Entity& text);
	[[nodiscard]] static V2_int GetSize(
		const std::string& content, const ResourceHandle& font_key, const FontSize& font_size = {}
	);
	[[nodiscard]] V2_int GetSize() const;

	void RecreateTexture();

	template <typename T>
	Text& SetParameter(const T& value, bool recreate_texture = true) {
		static_assert(tt::is_any_of_v<
					  T, ResourceHandle, TextContent, TextColor, FontStyle, FontRenderMode,
					  FontSize, TextLineSkip, TextShadingColor, TextWrapAfter, TextOutline,
					  TextJustify>);
		if (!Has<T>()) {
			Add<T>(value);
			if (recreate_texture) {
				RecreateTexture();
			}
			return *this;
		}
		T& t{ Get<T>() };
		if (t == value) {
			return *this;
		}
		t = value;
		if (recreate_texture) {
			RecreateTexture();
		}
		return *this;
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
};

// @param font_key Default: {} corresponds to the default engine font (use
// game.font.SetDefault(...) to change.
Text CreateText(
	Scene& scene, const TextContent& content, const TextColor& text_color = {},
	const ResourceHandle& font_key = {}
);

PTGN_SERIALIZER_REGISTER_ENUM(
	TextJustify, { { TextJustify::Left, "left" },
				   { TextJustify::Center, "center" },
				   { TextJustify::Right, "right" } }
);

} // namespace ptgn