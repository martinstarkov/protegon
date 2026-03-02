#include "runtime/graphics/text.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "app/context.h"
#include "core/assert.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/texture.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/font_system.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"

namespace ptgn {

static V2_float GetScale(const Scene& scene) {
	// TODO: Switch to using scene camera scale?
	auto scale{ scene.app().renderer.GetScale() };
	PTGN_ASSERT(scale.BothAboveZero());
	return scale;
}

static float FontSizeToHD(float font_size, const Scene& scene) {
	auto render_target_scale{ GetScale(scene) };
	font_size = font_size * render_target_scale.y;
	return font_size;
}

Text::Text(Entity entity) : Entity{ entity } {}

void Text::Draw(
	Renderer& renderer, Entity entity, V2_int text_size, Color additional_tint,
	Origin offset_origin, V2_float offset_size
) {
	Text text{ entity };

	if (!text.Has<impl::TextContent>()) {
		return;
	}

	if (text.Get<impl::TextContent>().GetValue().empty()) {
		return;
	}

	if (text.Has<impl::TextColor>() && text.Get<impl::TextColor>().a == 0) {
		return;
	}

	impl::Tint tint{ GetTint(text) };
	Transform transform{ GetDrawTransform(text) };

	if (tint.a == 0 || additional_tint.a == 0) {
		return;
	}

	// Offset text so it is centered on the offset origin and size.
	auto offset{ -GetOriginOffset(offset_origin, offset_size * Abs(transform.GetScale())) };
	transform.Translate(offset);

	if (bool is_hd{ text.IsHD() }) {
		auto scene_scale{ GetScale(text.GetScene()) };

		transform.Scale(transform.GetScale() / scene_scale);

		if (text.GetFontSize(is_hd) != text.Get<impl::HDFontSize>()) {
			Text::RecreateTexture(text);
		}
	}

	const auto& text_texture{ text.Get<Texture>() };

	if (!text_texture) {
		return;
	}

	V2_int size{ text_size };

	// If the text texture size for any text_size dimension that is zero.
	if (size.HasZero()) {
		V2_int texture_size{ text_texture.GetSize() };
		if (!size.x) {
			size.x = texture_size.x;
		}
		if (!size.y) {
			size.y = texture_size.y;
		}
	}

	auto texture_coordinates{ GetTextureCoordinates(text, false) };

	Color text_tint{ additional_tint.Normalized() * tint.Normalized() };

	impl::DrawQuadTexture(
		renderer, text_texture, transform, size, GetDrawOrigin(text), text_tint, GetDepth(text),
		GetBlendMode(text), texture_coordinates
	);
}

void Text::Draw(Renderer& renderer, Entity text) {
	// This wrapper exists so that buttons can draw offset text.
	Draw(renderer, text, V2_float{}, color::White, Origin::Center, V2_float{});
}

void Text::RecreateTexture(Entity entity) {
	Text text{ entity };
	auto content{ text.GetContent() };
	auto color{ text.GetColor() };
	auto font_size{ text.GetFontSize(text.IsHD()) };
	auto font{ text.GetFont() };
	auto properties{ text.GetProperties() };

	RecreateTexture(text, content, color, font_size, font, properties);
}

void Text::RecreateTexture(
	Entity text, std::string_view content, Color text_color, float font_size, Font font,
	const TextProperties& properties
) {
	// Cache the font size of the texture so that if HD resolution changes, the text is updated
	// before drawing.
	text.Add<impl::HDFontSize>(font_size);

	auto& asset{ text.GetScene().app().asset };

	auto texture{ asset.CreateTextTexture(content, text_color, font_size, font, properties) };

	text.Add<Texture>(texture);
}

void Text::SetProperties(Entity text, const TextProperties& properties) {
	SetProperties(text, properties, true);
}

void Text::SetProperties(Entity text, const TextProperties& properties, bool recreate_texture) {
	bool changed  = false;
	changed		 |= Text::SetParameter(text, properties.justify, false);
	changed		 |= Text::SetParameter(text, properties.line_skip, false);
	changed		 |= Text::SetParameter(text, properties.outline, false);
	changed		 |= Text::SetParameter(text, properties.render_mode, false);
	changed |= Text::SetParameter(text, impl::TextShadingColor{ properties.shading_color }, false);
	changed |= Text::SetParameter(text, properties.style, false);
	changed |= Text::SetParameter(text, impl::TextWrapAfter{ properties.wrap_after }, false);

	if (changed && recreate_texture) {
		Text::RecreateTexture(text);
	}
}

bool Text::IsHD() const {
	return Has<impl::HDText>();
}

Text& Text::SetHD(bool hd) {
	if (hd == IsHD()) {
		return *this;
	}
	if (hd) {
		Add<impl::HDText>();
	} else {
		Remove<impl::HDText>();
	}
	Text::RecreateTexture(*this);
	return *this;
}

Text& Text::SetFont(std::optional<Font> font) {
	Text::SetParameter(*this, font.value_or(Font{}));
	return *this;
}

Text& Text::SetContent(std::string_view content) {
	Text::SetParameter(*this, impl::TextContent{ content });
	return *this;
}

Text& Text::SetColor(Color color) {
	Text::SetParameter(*this, impl::TextColor{ color });
	return *this;
}

Text& Text::SetFontStyle(FontStyle font_style) {
	Text::SetParameter(*this, font_style);
	return *this;
}

Text& Text::SetFontSize(float pixels) {
	Text::SetParameter(*this, impl::FontSize{ pixels });
	return *this;
}

Text& Text::SetOutline(TextOutline outline) {
	Text::SetParameter(*this, FontRenderMode::Blended, false);
	Text::SetParameter(*this, outline, true);
	return *this;
}

Text& Text::SetFontRenderMode(FontRenderMode render_mode) {
	Text::SetParameter(*this, render_mode);
	return *this;
}

Text& Text::SetShadingColor(Color shading_color) {
	Text::SetParameter(*this, FontRenderMode::Shaded, false);
	Text::SetParameter(*this, impl::TextShadingColor{ shading_color }, true);
	return *this;
}

Text& Text::SetWrapAfter(std::uint32_t pixels) {
	Text::SetParameter(*this, impl::TextWrapAfter{ pixels });
	return *this;
}

Text& Text::SetLineSkip(TextLineSkip pixels) {
	Text::SetParameter(*this, pixels);
	return *this;
}

Text& Text::SetJustify(TextJustify text_justify) {
	Text::SetParameter(*this, text_justify);
	return *this;
}

Font Text::GetFont() const {
	return Text::GetParameter(*this, Font{});
}

std::string Text::GetContent() const {
	return Text::GetParameter(*this, impl::TextContent{});
}

Color Text::GetColor() const {
	return Text::GetParameter(*this, impl::TextColor{});
}

FontStyle Text::GetFontStyle() const {
	return Text::GetParameter(*this, FontStyle{});
}

FontRenderMode Text::GetFontRenderMode() const {
	return Text::GetParameter(*this, FontRenderMode{});
}

Color Text::GetShadingColor() const {
	return Text::GetParameter(*this, impl::TextShadingColor{});
}

TextJustify Text::GetJustify() const {
	return Text::GetParameter(*this, TextJustify{});
}

float Text::GetFontSize(bool hd) const {
	const auto& font_size{ Text::GetParameter(*this, impl::FontSize{}) };
	if (hd) {
		const auto& scene{ GetScene() };
		return FontSizeToHD(font_size, scene);
	}
	return font_size;
}

V2_int Text::GetSize(std::string_view content) const {
	return GetSize(content, GetFont(), GetFontSize(IsHD()));
}

V2_int Text::GetSize() const {
	return GetSize(
		Text::GetParameter(*this, impl::TextContent{}), Text::GetParameter(*this, Font{}),
		GetFontSize(IsHD())
	);
}

V2_int Text::GetSize(std::string_view content, Font font, std::optional<float> font_size) const {
	return GetScene().app().font.GetSize(font, content, font_size);
}

TextProperties Text::GetProperties() const {
	TextProperties properties;
	properties.justify		 = Text::GetParameter(*this, TextJustify{});
	properties.line_skip	 = Text::GetParameter(*this, TextLineSkip{});
	properties.outline		 = Text::GetParameter(*this, TextOutline{});
	properties.render_mode	 = Text::GetParameter(*this, FontRenderMode{});
	properties.shading_color = Text::GetParameter(*this, impl::TextShadingColor{});
	properties.style		 = Text::GetParameter(*this, FontStyle{});
	properties.wrap_after	 = Text::GetParameter(*this, impl::TextWrapAfter{});
	return properties;
}

Text CreateText(
	Scene& scene, std::string_view content, Color text_color, std::optional<float> font_size,
	std::variant<std::monostate, Font, std::string_view> font, const TextProperties& properties
) {
	std::optional<Font> resolved_font;

	std::visit(
		[&](auto&& arg) {
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, std::monostate>) {
				// Default engine font
				resolved_font = std::nullopt;
			} else if constexpr (std::is_same_v<T, Font>) {
				resolved_font = arg;
			} else if constexpr (std::is_same_v<T, std::string_view>) {
				PTGN_ASSERT(
					scene.app().asset.HasFont(arg),
					"Font key must be loaded in the asset manager before creating text"
				);

				resolved_font = *scene.app().asset.GetFont(arg);
			}
		},
		font
	);

	Text text{ scene.CreateEntity() };
	text.Add<Texture>();
	SetDraw<Text>(text);
	Show(text, false);
	text.Add<impl::HDText>();
	Text::SetParameter(text, impl::TextContent{ content }, false);
	Text::SetParameter(text, impl::TextColor{ text_color }, false);
	Text::SetParameter(text, resolved_font.value_or(Font{}), false);
	Text::SetParameter(text, impl::FontSize{ font_size.value_or(kDefaultFontSize) }, false);
	Text::SetProperties(text, properties, true);
	return text;
}

} // namespace ptgn