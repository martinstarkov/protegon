#include "runtime/graphics/text.h"

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "app/context.h"
#include "core/assert.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/texture.h"
#include "runtime/asset/asset.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/font_system.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/render_context.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace impl {

float GetTextScale(const Scene& scene, const std::optional<Camera>& camera) {
	return impl::GetCameraParentRenderTargetScale(scene, camera).y;
}

std::optional<float> ApplyHDTextScaling(
	bool hd, Transform& transform, const Scene& scene, const std::optional<Camera>& camera
) {
	if (hd) {
		auto scale{ impl::GetCameraParentRenderTargetScale(scene, camera) };

		PTGN_ASSERT(!scale.HasZero(), "Scale cannot have a zero component");

		transform.Scale(transform.GetScale() / scale);

		return impl::GetTextScale(scene, camera);
	}

	return std::nullopt;
}

} // namespace impl

Text::Text(Entity entity) : Entity{ entity } {}

void Text::Draw(
	DrawContext& renderer, Entity entity, V2_int text_size, Color additional_tint,
	Origin offset_origin, V2_float offset_size, Camera camera
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

	if (auto hd_scale{ impl::ApplyHDTextScaling(text.IsHD(), transform, text.GetScene(), camera) };
		hd_scale.has_value() &&
		text.GetFontSize() * hd_scale.value() != text.Get<impl::HDFontSize>()) {
		Text::RecreateTexture(text, camera);
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

	auto tex_coords{ GetTextureCoordinates(text, false) };

	Color text_tint{ additional_tint.Normalized() * tint.Normalized() };
	auto blend_mode{ GetBlendMode(text) };
	auto draw_origin{ GetDrawOrigin(text) };
	auto depth{ GetDepth(text) };

	renderer.DrawTexture(
		text_texture, transform, size, draw_origin, text_tint, depth, tex_coords, blend_mode
	);
}

void Text::Draw(DrawContext& renderer, Entity text, Camera camera) {
	// This wrapper exists so that buttons can draw offset text.
	Draw(renderer, text, V2_float{}, color::White, Origin::Center, V2_float{}, camera);
}

void Text::RecreateTexture(Entity entity, const std::optional<Camera>& camera) {
	Text text{ entity };
	auto content{ text.GetContent() };
	auto color{ text.GetColor() };
	std::optional<float> hd_scale;

	if (text.IsHD()) {
		hd_scale = impl::GetTextScale(text.GetScene(), camera);
	}

	auto font_size{ text.GetFontSize() };
	auto font{ text.GetFont() };
	auto properties{ text.GetProperties() };

	RecreateTexture(text, content, color, font_size, font, properties, hd_scale);
}

void Text::RecreateTexture(
	Entity text, std::string_view content, Color text_color, FontSize font_size, FontOrKey font,
	const TextProperties& properties, std::optional<float> hd_scale
) {
	// Cache the font size of the texture so that if HD resolution changes, the text is updated
	// before drawing.
	text.Add<impl::HDFontSize>(font_size);

	auto& asset{ text.GetScene().app().asset };

	auto texture{
		asset.CreateTextTexture(content, text_color, font_size, font, properties, hd_scale)
	};

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
		Text::RecreateTexture(text, std::nullopt);
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
	Text::RecreateTexture(*this, std::nullopt);
	return *this;
}

Text& Text::SetFont(FontOrKey font) {
	const auto& scene{ GetScene() };
	auto resolved_font{ font.Get(scene.app().asset) };
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
	return GetScene().app().font.GetSize(font, text_content, font_size);
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
	Scene& scene, std::string_view text_content, Color text_color, FontSize font_size,
	FontOrKey font, const TextProperties& properties
) {
	auto resolved_font{ font.Get(scene.app().asset) };

	Text text{ scene.CreateEntity() };
	text.Add<Texture>();
	SetDraw<Text>(text);
	Show(text, false);
	text.Add<impl::HDText>();
	Text::SetParameter(text, impl::TextContent{ text_content }, false);
	Text::SetParameter(text, impl::TextColor{ text_color }, false);
	Text::SetParameter(text, resolved_font, false);
	Text::SetParameter(text, font_size, false);
	Text::SetProperties(text, properties, true);
	return text;
}

std::ostream& operator<<(std::ostream& os, TextJustify text_justify) {
	switch (text_justify) {
		using enum TextJustify;
		case Left:	 return os << "Left";
		case Center: return os << "Center";
		case Right:	 return os << "Right";
		default:	 PTGN_ERROR("Unknown TextJustify: ", std::to_underlying(text_justify));
	}
}

} // namespace ptgn