#pragma once

#include <optional>
#include <string_view>

#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/text/font_style.h"
#include "renderer/text/text_glyph.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class DrawContext;
class Scene;
class AssetManager;

namespace impl {

struct TextEditState {
	std::size_t current_run_index{ 0 };
};

[[nodiscard]] ResolvedTextRun ResolveTextRun(AssetManager& asset_manager, const TextRun& text_run);

[[nodiscard]] ResolvedStyledText ResolveStyledText(
	AssetManager& asset_manager, const StyledText& styled_text
);

[[nodiscard]] TextLayout BuildTextLayout(
	AssetManager& asset_manager, const StyledText& styled_text, const TextBox& box
);

} // namespace impl

class Text : public Entity {
public:
	Text() = default;
	explicit Text(Entity entity);

	static void Draw(DrawContext& ctx, Entity entity);

	/// @brief Clears all text content and resets the current run index to 0.
	Text& Clear();

	/// @brief Selects the text run at the specified index. If the index is out of bounds, it will
	/// select the last text run.
	Text& Select(std::size_t index);

	/// @brief Appends and selects the content as the current text run.
	Text& Content(std::string_view content);

	/// @brief Appends and selects the styled text as the current text run.
	Text& Content(StyledText styled_text);

	Text& Box(Rect text_rect);
	Text& Box(const TextBox& text_box);

	Text& Align(Origin origin);
	Text& Align(Alignment alignment);
	Text& Align(HorizontalAlign horizontal, VerticalAlign vertical);
	Text& HorizontalAlign(HorizontalAlign align);
	Text& VerticalAlign(VerticalAlign align);

	/// @brief Removes explicit alignment overrides and returns to alignment derived
	/// from the current draw origin.
	Text& ClearAlignment();

	/// @brief Determines how text is wrapped to the next line when it exceeds the width of the text
	/// box.
	Text& Wrap(WrapMode mode);

	/// @brief Sets more specific rules for how text is wrapped to the next line when it exceeds the
	/// width of the text box.
	Text& WrapSettings(const ptgn::WrapSettings& settings);

	/// @brief Determines how text is handled when it exceeds the height of the text box.
	Text& Overflow(OverflowMode mode);

	/// @brief Useful for something like a scrollable text box where you want to clip the text to
	/// the box, but still allow the user to scroll the text outside of the box.
	/// Rectangle is positioned relative to the text's transform.
	Text& Clip(Rect rect, TextClipMode mode = TextClipMode::Clip);

	/// @brief Removes any clipping that was previously set.
	Text& ClearClip();

	/// @brief Sets the number of glyphs to reveal. If the count is greater than the total number of
	/// glyphs, all glyphs will be revealed.
	Text& Reveal(std::size_t glyph_count);

	/// @brief Removes any previously set reveal restrictions.
	Text& RevealAll();

	/// @brief Sets the fraction of the total glyphs to reveal. Clamped to range: [0.0, 1.0]. 0.0 =
	/// no glyphs revealed, 1.0 = all glyphs revealed.
	Text& RevealFraction(float fraction);

	/// @brief If true, leading and trailing spaces will be trimmed and consecutive whitespace
	/// characters will be collapsed into a single space across all text runs. Useful for processing
	/// user entered text or different localization strings.
	Text& CollapseSpaces(bool collapse = true);

	/// @brief By default, the last line of text is not justified. If true, the last line will be
	/// justified if the alignment is set to justify. This is useful for text that is not expected
	/// to be a paragraph, such as a single line of text.
	Text& JustifyLastLine(bool justify = true);

	/// @brief Kerning adjusts spacing between specific glyph pairs based on the font's kerning
	/// data.
	/// @param multiplier The multiplier for the kerning adjustment.
	/// 1.0 uses the font's kerning unchanged.
	/// 0.0 disables kerning.
	/// Values above 1.0 exaggerate kerning adjustments.
	Text& Kerning(float multiplier);

	/// @brief Tracking adds a uniform amount of spacing between all adjacent glyphs.
	/// The value is measured in rendered text pixels after font scaling.
	/// Positive values spread glyphs apart; negative values bring glyphs closer.
	Text& Tracking(float spacing);

	/// @brief Sets the number of space columns between tab stops.
	/// A tab advances to the next multiple of this many spaces.
	Text& TabWidth(std::size_t spaces);

	Text& LineSpacing(float line_spacing);

	Text& MaxLines(std::size_t max_lines);

	Text& ScaleToFit(float min_scale, float max_scale = 1.0f);

	Text& Font(std::string_view font_key = {});
	Text& Color(ptgn::Color color);
	Text& Size(float font_size);

	Text& Style(FontStyle flags);
	Text& Bold(bool enabled = true, float weight = kDefaultBoldWeight);
	Text& Italic(bool enabled = true);
	Text& Underline(bool enabled = true);
	Text& Strikethrough(bool enabled = true);

	Text& Outline(ptgn::Color color, float width, float softness = 1.0f);

	Text& Shadow(ptgn::Color color, V2_float offset, float softness = 1.0f);
	Text& Shadow(ptgn::Color color, V2_float offset, float width, float softness);

	Text& OuterGlow(ptgn::Color color, float width, float softness = 1.0f);
	Text& InnerGlow(ptgn::Color color, float width, float softness = 1.0f);

	Text& ClearSdfEffects();

	Text& Effect(
		GlyphEffectType type, float amplitude, float frequency, float speed, float phase = 0.0f
	);

	/// @return Final logical size of the laid out text.
	V2_float GetSize() const;

	/// @return Final text content bounds in the entity's local coordinate space,
	/// after applying the text draw origin.
	Rect GetBounds() const;

	/// @return The measured size of the text, which may be larger than the box if the text is
	/// clipped, ellipsized, or truncated.
	[[nodiscard]] TextMeasurement Measure() const;

	/// @return The number of glyphs that are currently set to be revealed or
	/// std::size_t::max() if no reveal is active.
	std::size_t GetRevealGlyphCount() const;

	/// @return True if all glyphs are currently revealed or no reveal restrictions are set.
	bool IsFullyRevealed() const;

	const StyledText& GetStyledText() const;
	const TextBox& GetTextBox() const;

	/// @return The text layout for the current styled text and text box. If the layout is not up to
	/// date, it will be rebuilt before returning.
	const TextLayout& GetLayout() const;

private:
	TextRun& CurrentRun();
	void InvalidateLayout();
};

Text CreateText(
	Scene& scene, Transform transform = {}, StyledText styled_text = {},
	Origin origin = Origin::Center
);

Text CreateText(
	Scene& scene, Transform transform, std::string_view content, Color color,
	float font_size = kDefaultFontSize, Origin origin = Origin::Center,
	std::string_view font = kDefaultFont
);

PTGN_REGISTER_DRAWABLE(Text);

} // namespace ptgn
