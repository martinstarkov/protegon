#include "runtime/graphics/text/text.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/font_system.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

Text::Text(Entity entity) : Entity{ entity } {}

void Text::Draw(
	DrawContext& renderer, Entity entity, V2_int text_size, Color additional_tint,
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
	const auto transform_scale{ transform.GetScale() };
	auto scaled_offset{ offset_size * Abs(transform_scale) };
	V2_float offset{ -GetOriginOffset(offset_origin, scaled_offset) };
	transform.Translate(offset);

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

	auto tex_coords{ GetTextureCoordinates(text, false) };

	Color text_tint{ additional_tint.Normalized() * tint.Normalized() };
	auto blend_mode{ GetBlendMode(text) };
	auto draw_origin{ GetDrawOrigin(text) };
	auto depth{ GetDepth(text) };

	// NOSONAR
	// Enable to see outline of text:
	// entity.GetScene().ctx().debug.DrawShape(
	//	Rect{ size }, transform, color::Purple, {}, draw_origin
	//);

	renderer.DrawTexture(
		text_texture, transform, depth, size, draw_origin, text_tint, tex_coords, blend_mode,
		text.GetUUID()
	);
}

void Text::Draw(DrawContext& renderer, Entity text) {
	// This wrapper exists so that buttons can draw offset text.
	Draw(renderer, text, V2_float{}, color::White, Origin::Center, V2_float{});
}

void Text::RecreateTexture(Entity entity) {
	Text text{ entity };
	auto content{ text.GetContent() };
	auto color{ text.GetColor() };

	auto font_size{ text.GetFontSize() };
	auto font{ text.GetFont() };
	auto properties{ text.GetProperties() };

	RecreateTexture(text, content, color, font_size, font, properties);
}

void Text::RecreateTexture(
	Entity text, std::string_view content, Color text_color, FontSize font_size, FontOrKey font,
	const TextProperties& properties
) {
	auto& asset{ text.GetScene().ctx().asset };

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

Text& Text::SetFont(FontOrKey font) {
	const auto& scene{ GetScene() };
	auto resolved_font{ font.Get(scene.ctx().asset) };
	Text::SetParameter(*this, resolved_font);
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

Text& Text::SetFontSize(FontSize font_size) {
	Text::SetParameter(*this, font_size);
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

FontSize Text::GetFontSize() const {
	return Text::GetParameter(*this, FontSize{});
}

V2_int Text::GetSize() const {
	return GetSize(
		Text::GetParameter(*this, impl::TextContent{}), Text::GetParameter(*this, Font{}),
		GetFontSize()
	);
}

V2_int Text::GetSize(std::string_view text_content, FontOrKey font, FontSize font_size) const {
	return GetScene().ctx().font.GetSize(font, text_content, font_size);
}

V2_int Text::GetSize(std::string_view text_content) const {
	return GetSize(text_content, GetFont(), GetFontSize());
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
	Scene& scene, V2_float position, std::string_view text_content, Color text_color,
	FontSize font_size, FontOrKey font, Origin draw_origin, const TextProperties& properties
) {
	auto resolved_font{ font.Get(scene.ctx().asset) };

	Text text{ scene.CreateEntity() };
	text.Add<Texture>();
	SetPosition(text, position);
	SetDrawOrigin(text, draw_origin);
	SetDraw<Text>(text);
	Show(text, false);
	Text::SetParameter(text, impl::TextContent{ text_content }, false);
	Text::SetParameter(text, impl::TextColor{ text_color }, false);
	Text::SetParameter(text, resolved_font, false);
	Text::SetParameter(text, font_size, false);
	Text::SetProperties(text, properties, true);
	return text;
}

} // namespace ptgn