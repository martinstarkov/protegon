#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/text.h"
#include "serialization/json/enum.h"
#include "serialization/json/serialize.h"

namespace ptgn {

class RenderContext;
class Scene;

namespace impl {

struct HDText {};

struct HDFontSize : public FontSize {
	using FontSize::FontSize;
};

struct TextContent : public StringComponent {
	using StringComponent::StringComponent;
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

} // namespace impl

struct TextOutline {
	std::int32_t width{ 0 };
	Color color;

	bool operator==(const TextOutline&) const = default;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(TextOutline, width, color)
};

enum class TextJustify {
	Left   = 0, // TTF_HORIZONTAL_ALIGN_LEFT
	Center = 1, // TTF_HORIZONTAL_ALIGN_CENTER
	Right  = 2	// TTF_HORIZONTAL_ALIGN_RIGHT
};

PTGN_SERIALIZE_ENUM(
	TextJustify, { { TextJustify::Left, "left" },
				   { TextJustify::Center, "center" },
				   { TextJustify::Right, "right" } }
);

struct TextLineSkip {
	TextLineSkip() = default;

	TextLineSkip(std::optional<std::int32_t> value) : value_{ value } {}

	operator std::optional<std::int32_t>() const {
		return value_;
	}

	[[nodiscard]] std::optional<std::int32_t> GetValue() const {
		return value_;
	}

	[[nodiscard]] std::optional<std::int32_t>& GetValue() {
		return value_;
	}

	bool operator==(const TextLineSkip&) const = default;

	// TODO: Fix serialization.
	// PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(TextLineSkip, value_.value_or(0))

private:
	std::optional<std::int32_t> value_{};
};

struct TextProperties {
	FontStyle style{};
	TextJustify justify{};
	TextLineSkip line_skip{};
	std::uint32_t wrap_after{ 0 };
	FontRenderMode render_mode{};
	TextOutline outline{};
	Color shading_color{ color::White };

	// TODO: Serialize line_skip once that is fixed.
	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		TextProperties, style, justify, wrap_after, render_mode, outline, shading_color // line_skip
	)
};

namespace impl {

template <typename T>
concept TextParameter = IsAnyOf<
	T, TextContent, TextColor, FontStyle, Font, FontRenderMode, FontSize, TextLineSkip,
	TextShadingColor, TextWrapAfter, TextOutline, TextJustify>;

} // namespace impl

class Text : public Entity {
public:
	Text() = default;
	explicit Text(Entity entity);

	static void Draw(
		RenderContext& renderer, Entity text, V2_int text_size, Color additional_tint,
		Origin offset_origin, V2_float offset_size
	);

	static void Draw(RenderContext& renderer, Entity entity);

	/// @return True if the text is rendered in high definition, false otherwise.
	[[nodiscard]] bool IsHD() const;

	[[nodiscard]] Font GetFont() const;
	[[nodiscard]] std::string GetContent() const;
	[[nodiscard]] Color GetColor() const;
	[[nodiscard]] FontStyle GetFontStyle() const;
	[[nodiscard]] FontRenderMode GetFontRenderMode() const;
	[[nodiscard]] Color GetShadingColor() const;
	[[nodiscard]] TextJustify GetJustify() const;

	/// @param hd If true, returns font size scaled to high definition.
	/// @param camera The camera relative to which an hd font size is retrieved. Only applicable if
	/// hd is true.
	[[nodiscard]] float GetFontSize(bool hd = false) const;

	/// @param camera The camera relative to which an hd text size is retrieved. Only applicable if
	/// text is hd
	/// @return The unscaled size of the text texture given the current content and font.
	[[nodiscard]] V2_int GetSize() const;

	/// @param camera The camera relative to which an hd text size is retrieved. Only applicable if
	/// text is hd
	/// @return The unscaled size of the text texture given the specified content.
	[[nodiscard]] V2_int GetSize(std::string_view content) const;

	[[nodiscard]] V2_int GetSize(
		std::string_view content, Font font, std::optional<float> font_size = {}
	) const;

	[[nodiscard]] TextProperties GetProperties() const;

	/// Set text to be rendered in high definition instead of natively scaling to its camera.
	Text& SetHD(bool hd = true);

	/// @param font Default {} corresponds to the default engine font.
	Text& SetFont(std::optional<Font> font = {});
	Text& SetContent(std::string_view content);
	Text& SetColor(Color color);

	/// To create text with multiple FontStyles, simply use &&, e.g.
	/// FontStyle::Italic && FontStyle::Bold
	Text& SetFontStyle(FontStyle font_style);

	/// Set the point size of text. Infinity will use the current point size of the font.
	Text& SetFontSize(float pt_size);

	/// Note: This function will implicitly set font render mode to Blended as it is required.
	/// @param outline Setting outline.width to 0 will remove the text outline.
	Text& SetOutline(TextOutline outline);

	Text& SetFontRenderMode(FontRenderMode render_mode);

	/// Sets the background shading color for the text.
	/// Also sets the font render mode to FontRenderMode::Shaded.
	Text& SetShadingColor(Color shading_color);

	/// Text wrapped to multiple lines on line endings and on word boundaries if it extends beyond
	/// this pixel value. Setting pixels = 0 (default) will wrap only after newlines.
	Text& SetWrapAfter(std::uint32_t pixels);

	/// Set the spacing between lines of text. {} will use the current font line skip.
	Text& SetLineSkip(TextLineSkip pixels = {});

	/// Determines how text is justified.
	Text& SetJustify(TextJustify text_justify);

	static void SetProperties(Entity text, const TextProperties& properties);

	static void SetProperties(Entity text, const TextProperties& properties, bool recreate_texture);

	/// @return True if the parameter was changed.
	template <impl::TextParameter T>
	static bool SetParameter(Entity text, const T& value, bool recreate_texture = true) {
		if (!text.Has<T>()) {
			text.Add<T>(value);
			if (recreate_texture) {
				RecreateTexture(text);
			}
			return true;
		}
		T& t{ text.Get<T>() };
		if (t == value) {
			if (recreate_texture) {
				RecreateTexture(text);
			}
			return false;
		}
		t = value;
		if (recreate_texture) {
			RecreateTexture(text);
		}
		return true;
	}

private:
	// Using own properties.
	static void RecreateTexture(Entity text);

	// Using custom properties.
	static void RecreateTexture(
		Entity text, std::string_view text_content, Color text_color, float font_size, Font font,
		const TextProperties& properties
	);

	template <impl::TextParameter T>
	[[nodiscard]] static const T& GetParameter(Entity text, const T& default_value) {
		if (!text.Has<T>()) {
			return default_value;
		}
		return text.Get<T>();
	}
};

/// @param font Default {} corresponds to the default engine font.
Text CreateText(
	Scene& scene, std::string_view content, Color text_color = {},
	std::optional<float> font_size							  = {},
	std::variant<std::monostate, Font, std::string_view> font = {},
	const TextProperties& properties						  = {}
);

PTGN_REGISTER_DRAWABLE(Text);

} // namespace ptgn