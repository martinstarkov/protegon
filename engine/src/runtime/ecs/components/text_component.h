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

class Text : public Entity {
public:
	Text() = default;
	explicit Text(Entity entity);

	static void Draw(
		Renderer& renderer, Entity text, V2_int text_size, Color additional_tint,
		Origin offset_origin, V2_float offset_size
	);

	static void Draw(Renderer& renderer, Entity entity);

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
	static [[nodiscard]] const T& GetParameter(Entity text, const T& default_value) {
		if (!text.Has<T>()) {
			return default_value;
		}
		return text.Get<T>();
	}
};

/// @param font Default {} corresponds to the default engine font.
Text CreateText(
	Scene& scene, std::string_view content, Color text_color = {},
	std::optional<float> font_size = {}, std::optional<Font> font = {},
	const TextProperties& properties = {}
);

PTGN_REGISTER_DRAWABLE(Text);

} // namespace ptgn