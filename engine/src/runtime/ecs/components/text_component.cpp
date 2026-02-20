#include "runtime/ecs/components/text_component.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "app/context.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/primitives/font.h"
#include "renderer/primitives/text.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/components/camera_component.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace impl {

void TextDraw::RecreateTexture(Entity text, Entity camera) {
	auto content{ GetTextContent(text) };
	auto color{ GetTextColor(text) };
	auto font_size{ GetTextFontSize(text, IsTextHD(text), camera) };
	auto font{ GetTextFont(text) };
	auto properties{ GetTextProperties(text) };

	RecreateTexture(text, content, color, font_size, font, properties);
}

void TextDraw::RecreateTexture(
	Entity text, std::string_view content, Color text_color, float font_size, Font font,
	const TextProperties& properties
) {
	// Cache the font size of the texture so that if HD resolution changes, the text is updated
	// before drawing.
	text.Add<impl::HDFontSize>(font_size);

	auto& assets{ text.GetScene().app().assets };

	auto texture{ assets.CreateTextTexture(content, text_color, font_size, font, properties) };

	text.Add<Texture>(texture);
}

void SetTextProperties(Entity text, const TextProperties& properties, Entity camera) {
	SetTextProperties(text, properties, true, camera);
}

void SetTextProperties(
	Entity text, const TextProperties& properties, bool recreate_texture, Entity camera
) {
	bool changed  = false;
	changed		 |= SetTextParameter(text, properties.justify, false);
	changed		 |= SetTextParameter(text, properties.line_skip, false);
	changed		 |= SetTextParameter(text, properties.outline, false);
	changed		 |= SetTextParameter(text, properties.render_mode, false);
	changed |= SetTextParameter(text, impl::TextShadingColor{ properties.shading_color }, false);
	changed |= SetTextParameter(text, properties.style, false);
	changed |= SetTextParameter(text, impl::TextWrapAfter{ properties.wrap_after }, false);

	if (changed && recreate_texture) {
		TextDraw::RecreateTexture(text, camera);
	}
}

} // namespace impl

Entity SetTextHD(Entity text, bool hd, Entity camera) {
	if (hd == IsTextHD(text)) {
		return text;
	}
	if (hd) {
		text.Add<impl::HDText>();
	} else {
		text.Remove<impl::HDText>();
	}
	impl::TextDraw::RecreateTexture(text, camera);
	return text;
}

bool IsTextHD(Entity text) {
	return text.Has<impl::HDText>();
}

Entity SetTextFont(Entity text, std::optional<Font> font) {
	impl::SetTextParameter(text, font.value_or(Font{}));
	return text;
}

Entity SetTextContent(Entity text, std::string_view content) {
	impl::SetTextParameter(text, impl::TextContent{ content });
	return text;
}

Entity SetTextColor(Entity text, Color color) {
	impl::SetTextParameter(text, impl::TextColor{ color });
	return text;
}

Entity SetTextFontStyle(Entity text, FontStyle font_style) {
	impl::SetTextParameter(text, font_style);
	return text;
}

Entity SetTextFontSize(Entity text, float pixels) {
	impl::SetTextParameter(text, impl::FontSize{ pixels });
	return text;
}

Entity SetTextOutline(Entity text, TextOutline outline) {
	impl::SetTextParameter(text, FontRenderMode::Blended, false);
	impl::SetTextParameter(text, outline, true);
	return text;
}

Entity SetTextFontRenderMode(Entity text, FontRenderMode render_mode) {
	impl::SetTextParameter(text, render_mode);
	return text;
}

Entity SetTextShadingColor(Entity text, Color shading_color) {
	impl::SetTextParameter(text, FontRenderMode::Shaded, false);
	impl::SetTextParameter(text, impl::TextShadingColor{ shading_color }, true);
	return text;
}

Entity SetTextWrapAfter(Entity text, std::uint32_t pixels) {
	impl::SetTextParameter(text, impl::TextWrapAfter{ pixels });
	return text;
}

Entity SetTextLineSkip(Entity text, TextLineSkip pixels) {
	impl::SetTextParameter(text, pixels);
	return text;
}

Entity SetTextJustify(Entity text, TextJustify text_justify) {
	impl::SetTextParameter(text, text_justify);
	return text;
}

Font GetTextFont(Entity text) {
	return impl::GetTextParameter(text, Font{});
}

std::string GetTextContent(Entity text) {
	return impl::GetTextParameter(text, impl::TextContent{});
}

Color GetTextColor(Entity text) {
	return impl::GetTextParameter(text, impl::TextColor{});
}

FontStyle GetTextFontStyle(Entity text) {
	return impl::GetTextParameter(text, FontStyle{});
}

FontRenderMode GetTextFontRenderMode(Entity text) {
	return impl::GetTextParameter(text, FontRenderMode{});
}

Color GetTextShadingColor(Entity text) {
	return impl::GetTextParameter(text, impl::TextShadingColor{});
}

TextJustify GetTextJustify(Entity text) {
	return impl::GetTextParameter(text, TextJustify{});
}

static float FontSizeToHD(float font_size, const Scene& scene, Entity camera) {
	auto render_target_scale{ scene.GetRenderTargetScaleRelativeTo(camera) };
	font_size = font_size * render_target_scale.y;
	return font_size;
}

float GetTextFontSize(Entity text, bool hd, Entity camera) {
	const auto& font_size{ impl::GetTextParameter(text, impl::FontSize{}) };
	if (hd) {
		const auto& scene{ text.GetScene() };
		auto cam{ camera ? camera : GetCamera(text) };
		return FontSizeToHD(font_size, scene, cam);
	}
	return font_size;
}

V2_int GetTextSize(Entity text, std::string_view content, Entity camera) {
	return GetTextSize(
		text, content, GetTextFont(text), GetTextFontSize(text, IsTextHD(text), camera)
	);
}

V2_int GetTextSize(Entity text, Entity camera) {
	return GetTextSize(
		text, impl::GetTextParameter(text, impl::TextContent{}),
		impl::GetTextParameter(text, Font{}), GetTextFontSize(text, IsTextHD(text), camera)
	);
}

V2_int GetTextSize(
	Entity text, std::string_view content, Font font, std::optional<float> font_size
) {
	return text.GetScene().app().assets.GetFontSize(font, content, font_size);
}

TextProperties GetTextProperties(Entity text) {
	TextProperties properties;
	properties.justify		 = impl::GetTextParameter(text, TextJustify{});
	properties.line_skip	 = impl::GetTextParameter(text, TextLineSkip{});
	properties.outline		 = impl::GetTextParameter(text, TextOutline{});
	properties.render_mode	 = impl::GetTextParameter(text, FontRenderMode{});
	properties.shading_color = impl::GetTextParameter(text, impl::TextShadingColor{});
	properties.style		 = impl::GetTextParameter(text, FontStyle{});
	properties.wrap_after	 = impl::GetTextParameter(text, impl::TextWrapAfter{});
	return properties;
}

Entity CreateText(
	Scene& scene, std::string_view content, Color text_color, std::optional<float> font_size,
	std::optional<Font> font, const TextProperties& properties
) {
	auto text{ scene.CreateEntity() };
	text.Add<Texture>();
	SetDraw<impl::TextDraw>(text);
	Show(text);
	text.Add<impl::HDText>();
	impl::SetTextParameter(text, impl::TextContent{ content }, false);
	impl::SetTextParameter(text, impl::TextColor{ text_color }, false);
	impl::SetTextParameter(text, font.value_or(Font{}), false);
	impl::SetTextParameter(text, impl::FontSize{ font_size.value_or(default_font_size) }, false);
	impl::SetTextProperties(text, properties, true, {});
	return text;
}

} // namespace ptgn