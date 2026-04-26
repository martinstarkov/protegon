#include "runtime/graphics/text/text.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "font_data.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/font_system.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "text_system.h"

namespace ptgn {

Text::Text(Entity entity) : Entity{ entity } {}

void Text::Draw(
	DrawContext& renderer, Entity entity, V2_int text_size, Color additional_tint,
	Origin offset_origin, V2_float offset_size
) {
	// TODO: Move out.
	// static TextSystem text_system;
	// static impl::MsdfFontData msdf_font = {
	//	renderer.renderer_, "assets/fonts/LiberationSans-Regular.ttf", 0, {}
	//};
	/*draw_context.DrawTexture(
		msdf_font.GetAtlasTexture(), {}, 0.0f, msdf_font.GetAtlasSize(), Origin::Center,
		color::White, impl::GetDefaultTextureCoordinates<false>(), std::nullopt, -1
	);*/

	// text_system.DrawText(renderer, {}, &msdf_font);

	/*
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
	*/
}

void Text::Draw(DrawContext& renderer, Entity text) {
	// This wrapper exists so that buttons can draw offset text.
	Draw(renderer, text, V2_float{}, color::White, Origin::Center, V2_float{});
}

Text& Text::SetFont(std::string_view font_key) {
	const auto& scene{ GetScene() };
	const auto& assets{ scene.ctx().asset };
	auto font{ assets.Get<Font>(font_key) };
	Add<Font>(font);
	//  TODO: fix.
	return *this;
}

Text& Text::SetContent(std::string_view content) {
	// TODO: fix.
	return *this;
}

Text& Text::SetColor(Color color) {
	// TODO: fix.
	return *this;
}

Text& Text::SetFontStyle(FontStyle font_style) {
	// TODO: fix.
	return *this;
}

Text& Text::SetFontSize(FontSize font_size) {
	// TODO: fix.
	return *this;
}

Text& Text::SetWrapMode(WrapMode wrap_mode) {
	// TODO: fix.
	return *this;
}

Text& Text::SetHorizontalAlign(HorizontalAlign horizontal_align) {
	// TODO: fix.
	return *this;
}

Text& Text::SetVerticalAlign(VerticalAlign vertical_align) {
	// TODO: fix.
	return *this;
}

Text& Text::SetOverflowMode(OverflowMode overflow_mode) {
	// TODO: fix.
	return *this;
}

Font Text::GetFont() const {
	// TODO: fix.
	return {};
}

std::string Text::GetFontKey() const {
	// TODO: fix.
	return {};
}

std::string Text::GetContent() const {
	// TODO: fix.
	return {};
}

Color Text::GetColor() const {
	// TODO: fix.
	return {};
}

FontStyle Text::GetFontStyle() const {
	// TODO: fix.
	return {};
}

WrapMode Text::GetWrapMode() const {
	// TODO: fix.
	return {};
}

HorizontalAlign Text::GetHorizontalAlign() const {
	// TODO: fix.
	return {};
}

VerticalAlign Text::GetVerticalAlign() const {
	// TODO: fix.
	return {};
}

OverflowMode Text::GetOverflowMode() const {
	// TODO: fix.
	return {};
}

FontSize Text::GetFontSize() const {
	// TODO: fix.
	return {};
}

V2_int Text::GetSize() const {
	return GetSize(GetContent(), GetFontKey(), GetFontSize());
}

V2_int Text::GetSize(std::string_view text_content) const {
	return GetSize(text_content, GetFontKey(), GetFontSize());
}

V2_int Text::GetSize(std::string_view text_content, std::string_view font_key, FontSize font_size)
	const {
	// TODO: Fix.
	// return GetScene().ctx().font.GetSize(font_key, text_content, font_size);
	return {};
}

Text CreateText(
	Scene& scene, V2_float position, std::string_view text_content, Color text_color,
	FontSize font_size, std::string_view font_key, Origin draw_origin
) {
	Text text{ scene.CreateEntity() };
	text.Add<Texture>();
	SetPosition(text, position);
	SetDrawOrigin(text, draw_origin);
	SetDraw<Text>(text);
	Show(text, false);
	text.SetFont(font_key);
	return text;
}

} // namespace ptgn