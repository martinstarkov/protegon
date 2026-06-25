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
class SceneCamera;

namespace impl {

struct TextAlignmentOverride {
	bool horizontal{ false };
	bool vertical{ false };
};

struct TextEditState {
	std::size_t current_run_index{ 0 };
};

[[nodiscard]] ResolvedTextRun ResolveTextRun(AssetManager& asset_manager, const TextRun& text_run);

[[nodiscard]] ResolvedStyledText ResolveStyledText(
	AssetManager& asset_manager, const StyledText& styled_text
);

void DrawDebugTextBoundingBoxes(
	Scene& scene, const std::optional<SceneCamera>& camera, const impl::EntityFilterFunc& filter
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

	TextBox& GetTextBox();
	const TextBox& GetTextBox() const;

	Text& Clear();

	/// @brief Appends/selects a text segment and makes it the current run.
	Text& Content(std::string_view content);

	Text& Content(StyledText styled_text);

	Text& Select(std::size_t index);

	Text& Box(Rect text_box);
	Text& Box(const TextBox& box);

	Text& Reveal(std::size_t glyph_count);
	Text& RevealAll();

	Text& Align(HorizontalAlign horizontal, VerticalAlign vertical);
	Text& HorizontalAlign(HorizontalAlign align);
	Text& VerticalAlign(VerticalAlign align);

	/// @brief Removes any previously set alignment.
	Text& ClearAlignment();

	/// @brief Sets the number of space columns between tab stops.
	/// A tab advances to the next multiple of this many spaces.
	Text& TabWidth(std::size_t spaces);

	Text& Wrap(WrapMode mode);
	Text& Overflow(OverflowMode mode);

	/// @brief Useful for something like a scrollable text box where you want to clip the text to
	/// the box, but still allow the user to scroll the text outside of the box.
	Text& Clip(Rect rect, TextClipMode mode = TextClipMode::Clip);

	/// @brief Removes any clipping that was previously set.
	Text& ClearClip();

	/// @brief If true, leading and trailing spaces will be trimmed and consecutive whitespace
	/// characters will be collapsed into a single space across all text runs. Useful for processing
	/// user entered text or different localization strings.
	Text& CollapseSpaces(bool collapse = true);

	/// @brief By default, the last line of text is not justified. If true, the last line will be
	/// justified if the alignment is set to justify. This is useful for text that is not expected
	/// to be a paragraph, such as a single line of text.
	Text& JustifyLastLine(bool justify = true);

	/// @brief Only applicable for WrapMode::Word. If true, move the word to a new line, then split
	/// it across lines if it cannot fit on an empty line. If false, move the word to a new line,
	/// then allow it to overflow if it is still too wide.
	Text& AllowWordBreakInOverflow(bool allow = true);

	/// @brief Only used by WrapMode::Character.
	/// If true, inserts a hyphen at the end of a line when a word is split across lines.
	Text& InsertHyphenOnSplit(bool insert = true);

	/// @brief Only used by WrapMode::Character.
	/// If true, if a word is split and only a single letter remains on the current line, the
	/// entire word is moved to the next line instead.
	Text& PreventSingleLetterSplit(bool prevent = true);

	/// @brief Only used by WrapMode::Character.
	/// If true, if a word is split and fewer than three letters remain on the next line, the
	/// entire word is moved to the next line instead.
	Text& RequireThreeLetterRemainder(bool require = true);

	Text& MaxLines(std::size_t max_lines);
	Text& ScaleToFit(float min_scale, float max_scale = 1.0f);

	Text& Font(std::string_view font_key = {});
	Text& Color(ptgn::Color color);
	Text& Size(float font_size);

	/// @brief Kerning adjusts spacing between specific glyph pairs based on the font's kerning
	/// data.
	/// @param multiplier The multiplier for the kerning adjustment.
	/// 1.0 uses the font's kerning unchanged.
	/// 0.0 disables kerning.
	/// Values above 1.0 exaggerate kerning adjustments.
	Text& Kerning(float multiplier);

	/// @brief Tracking adds a uniform amount of spacing between all adjacent glyphs.
	/// The value is measured in rendered text pixels after font scaling.
	/// Positive values spread glyphs apart; negative values bring them closer.
	Text& Tracking(float spacing);

	Text& LineSpacing(float line_spacing);

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

	/// @return The total number of glyphs in the text, which may be more than the number of visible
	/// glyphs if the text is clipped, ellipsized, or truncated.
	std::size_t GetGlyphCount() const;

	/// @return The number of glyphs that are currently visible, which may be less than the total
	/// glyph count if the text is clipped, ellipsized, or truncated.
	std::size_t GetVisibleGlyphCount() const;

	/// @return The number of glyphs that are currently set to be revealed or
	/// std::size_t::max() if no reveal is active.
	std::size_t GetRevealGlyphCount() const;

	/// @return True if all glyphs are currently revealed or no reveal is active.
	bool IsFullyRevealed() const;

	/// @brief Sets the fraction of the total glyphs to reveal. Clamped to range: [0.0, 1.0]. 0.0 =
	/// no glyphs revealed, 1.0 = all glyphs revealed.
	Text& RevealFraction(float fraction);

	const TextLayout& GetLayout() const;

	StyledText& GetStyledText();
	const StyledText& GetStyledText() const;

private:
	friend class Button;

	void InvalidateLayout();

	void ApplyFallbackAlignment(Alignment alignment);

	TextRun& CurrentRun();
	const TextRun& CurrentRun() const;
};

Text CreateText(Scene& scene, Transform transform = {}, Origin draw_origin = Origin::Center);

PTGN_REGISTER_DRAWABLE(Text);

} // namespace ptgn