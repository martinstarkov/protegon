#pragma once

#include <string>
#include <string_view>

#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/text_style.h"

namespace ptgn {

class DrawContext;
class Scene;

class Text : public Entity {
public:
	Text() = default;
	explicit Text(Entity entity);

	static void Draw(
		DrawContext& ctx, Entity text, V2_int text_size, Color additional_tint,
		Origin offset_origin, V2_float offset_size
	);

	static void Draw(DrawContext& ctx, Entity entity);

	std::string GetFontKey() const;
	Font GetFont() const;
	std::string GetContent() const;
	Color GetColor() const;
	FontStyle GetFontStyle() const;
	WrapMode GetWrapMode() const;
	HorizontalAlign GetHorizontalAlign() const;
	VerticalAlign GetVerticalAlign() const;
	OverflowMode GetOverflowMode() const;

	FontSize GetFontSize() const;

	/// @return Size of this text's texture.
	V2_int GetSize() const;

	/// @return Texture size for the given text content using this text's font and size.
	V2_int GetSize(std::string_view text_content) const;

	/// @return Texture size for the given text content using the specified font and size.
	V2_int GetSize(
		std::string_view text_content, std::string_view font_key, FontSize font_size = {}
	) const;

	/// @param font_key Default {} corresponds to the default engine font.
	Text& SetFont(std::string_view font_key = {});
	Text& SetContent(std::string_view content);
	Text& SetColor(Color color);

	/// To create text with multiple FontStyles, simply use &&, e.g.
	/// FontStyle::Italic && FontStyle::Bold
	Text& SetFontStyle(FontStyle font_style);

	/// Set the font size of text. Default value will use default engine font.
	Text& SetFontSize(FontSize font_size = kDefaultFontSize);

	Text& SetWrapMode(WrapMode wrap_mode);
	Text& SetHorizontalAlign(HorizontalAlign horizontal_align);
	Text& SetVerticalAlign(VerticalAlign vertical_align);
	Text& SetOverflowMode(OverflowMode overflow_mode);
};

Text CreateText(
	Scene& scene, V2_float position, std::string_view text_content, Color text_color = color::White,
	FontSize font_size = {}, std::string_view font_key = {}, Origin draw_origin = Origin::Center
);

PTGN_REGISTER_DRAWABLE(Text);

} // namespace ptgn