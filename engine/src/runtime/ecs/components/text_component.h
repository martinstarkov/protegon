#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/primitives/font.h"
#include "renderer/primitives/text.h"
#include "runtime/ecs/components/drawable.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Renderer;
class Scene;

namespace impl {

void DrawText(
	Renderer& renderer, Entity text, V2_int text_size, Entity camera, Color additional_tint,
	Origin offset_origin, V2_float offset_size
);

class TextDraw {
public:
	static void Draw(Renderer& renderer, Entity entity);

	// Using own properties.
	static void RecreateTexture(Entity text, Entity camera);

	// Using custom properties.
	static void RecreateTexture(
		Entity text, std::string_view text_content, Color text_color, float font_size, Font font,
		const TextProperties& properties
	);
};

template <TextParameter T>
[[nodiscard]] const T& GetTextParameter(Entity text, const T& default_value) {
	if (!text.Has<T>()) {
		return default_value;
	}
	return text.Get<T>();
}

template <typename T>
[[nodiscard]] const T& GetTExtParameter(const T& default_value) {
	return GetTextParameter<T>(*this, default_value);
}

// @return True if the parameter was changed.
template <TextParameter T>
bool SetTextParameter(Entity text, const T& value, bool recreate_texture = true) {
	if (!text.Has<T>()) {
		text.Add<T>(value);
		if (recreate_texture) {
			TextDraw::RecreateTexture(text, {});
		}
		return true;
	}
	T& t{ text.Get<T>() };
	if (t == value) {
		if (recreate_texture) {
			TextDraw::RecreateTexture(text, {});
		}
		return false;
	}
	t = value;
	if (recreate_texture) {
		TextDraw::RecreateTexture(text, {});
	}
	return true;
}

void SetTextProperties(Entity text, const TextProperties& properties, Entity camera = {});

void SetTextProperties(
	Entity text, const TextProperties& properties, bool recreate_texture, Entity camera = {}
);

} // namespace impl

// Set text to be in high definition instead of natively scaling to its camera.
Entity SetTextHD(Entity text, bool hd = true, Entity camera = {});
[[nodiscard]] bool IsTextHD(Entity text);

// @param font Default {} corresponds to the default engine font.
Entity SetTextFont(Entity text, std::optional<Font> font = {});
Entity SetTextContent(Entity text, std::string_view content);
Entity SetTextColor(Entity text, Color color);

// To create text with multiple FontStyles, simply use &&, e.g.
// FontStyle::Italic && FontStyle::Bold
Entity SetTextFontStyle(Entity text, FontStyle font_style);

// Set the point size of text. Infinity will use the current point size of the font.
Entity SetTextFontSize(Entity text, float pt_size);

// Note: This function will implicitly set font render mode to Blended as it is required.
// @param outline Setting outline.width to 0 will remove the text outline.
Entity SetTextOutline(Entity text, TextOutline outline);

Entity SetTextFontRenderMode(Entity text, FontRenderMode render_mode);

// Sets the background shading color for the text.
// Also sets the font render mode to FontRenderMode::Shaded.
Entity SetTextShadingColor(Entity text, Color shading_color);

// text wrapped to multiple lines on line endings and on word boundaries if it extends beyond
// this pixel value. Setting pixels = 0 (default) will wrap only after newlines.
Entity SetTextWrapAfter(Entity text, std::uint32_t pixels);

// Set the spacing between lines of text. {} will use the current font line skip.
Entity SetTextLineSkip(Entity text, TextLineSkip pixels = {});

// Determines how text is justified.
Entity SetTextJustify(Entity text, TextJustify text_justify);

[[nodiscard]] Font GetTextFont(Entity text);
[[nodiscard]] std::string GetTextContent(Entity text);
[[nodiscard]] Color GetTextColor(Entity text);
[[nodiscard]] FontStyle GetTextFontStyle(Entity text);
[[nodiscard]] FontRenderMode GetTextFontRenderMode(Entity text);
[[nodiscard]] Color GetTextShadingColor(Entity text);
[[nodiscard]] TextJustify GetTextJustify(Entity text);

// @param hd If true, returns font size scaled to high definition.
// @param camera The camera relative to which an hd font size is retrieved. Only applicable if
// hd is true. If {}, uses the text's camera component, which may be the scene camera.
[[nodiscard]] float GetTextFontSize(Entity text, bool hd = false, Entity camera = {});

// @param camera The camera relative to which an hd text size is retrieved. Only applicable if
// text is hd. If {}, uses the text's camera component, which may be the scene camera.
// @return The unscaled size of the text texture given the current content and font.
[[nodiscard]] V2_int GetTextSize(Entity text, Entity camera = {});

// @param camera The camera relative to which an hd text size is retrieved. Only applicable if
// text is hd. If {}, uses the text's camera component, which may be the scene camera.
// @return The unscaled size of the text texture given the specified content.
[[nodiscard]] V2_int GetTextSize(Entity text, std::string_view content, Entity camera = {});

[[nodiscard]] V2_int GetTextSize(
	Entity text, std::string_view content, Font font, std::optional<float> font_size = {}
);

[[nodiscard]] TextProperties GetTextProperties(Entity text);

// @param font Default {} corresponds to the default engine font.
Entity CreateText(
	Scene& scene, std::string_view content, Color text_color = {},
	std::optional<float> font_size = {}, std::optional<Font> font = {},
	const TextProperties& properties = {}
);

PTGN_REGISTER_DRAWABLE(impl::TextDraw);

} // namespace ptgn