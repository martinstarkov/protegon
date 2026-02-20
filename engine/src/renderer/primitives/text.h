#pragma once

#include <cstdint>
#include <optional>

#include "core/component.h"
#include "core/graphics/color.h"
#include "core/util/concepts.h"
#include "renderer/primitives/font.h"
#include "serialization/json/enum.h"
#include "serialization/json/serialize.h"

namespace ptgn {

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

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(TextLineSkip, value_.value_or(0))

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

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		TextProperties, style, justify, line_skip, wrap_after, render_mode, outline, shading_color
	)
};

namespace impl {

template <typename T>
concept TextParameter = IsAnyOf<
	T, TextContent, TextColor, FontStyle, Font, FontRenderMode, FontSize, TextLineSkip,
	TextShadingColor, TextWrapAfter, TextOutline, TextJustify>;

} // namespace impl

} // namespace ptgn