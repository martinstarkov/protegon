#pragma once

#include <optional>
#include <string_view>

#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/text/font_style.h"
#include "renderer/text/glyph.h"
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

struct TextEditState {
	std::size_t current_run_index{ 0 };
};

void DrawDebugTextBoundingBoxes(
	Scene& scene, const std::optional<SceneCamera>& camera, const impl::EntityFilterFunc& filter
);

} // namespace impl

class Text : public Entity {
public:
	Text() = default;
	explicit Text(Entity entity);

	static void Draw(DrawContext& ctx, Entity entity);

	static void Draw(
		DrawContext& ctx, Entity text, V2_int text_size, Color additional_tint,
		Origin offset_origin, V2_float offset_size
	);

	StyledText& GetStyledText();
	const StyledText& GetStyledText() const;

	TextBox& GetTextBox();
	const TextBox& GetTextBox() const;

	Text& Clear();

	// Appends/selects a text segment and makes it the current run.
	Text& Content(std::string_view content);

	Text& Select(std::size_t index);

	Text& Box(Rect rect);

	Text& Reveal(std::size_t glyph_count);
	Text& RevealAll();

	Text& Align(HorizontalAlign horizontal, VerticalAlign vertical);
	Text& HorizontalAlign(HorizontalAlign align);
	Text& VerticalAlign(VerticalAlign align);

	Text& Wrap(WrapMode mode);
	Text& Overflow(OverflowMode mode);

	Text& Clip(Rect rect, TextClipMode mode = TextClipMode::ClipFullyOutside);

	Text& ClearClip();

	Text& CollapseSpaces(bool collapse = true);
	Text& JustifyLastLine(bool justify = true);
	Text& AllowWordBreakInOverflow(bool allow = true);

	Text& MaxLines(std::size_t max_lines);
	Text& ScaleToFit(float min_scale, float max_scale = 1.0f);

	Text& Font(std::string_view font_key = {});
	Text& Color(ptgn::Color color);
	Text& Size(float size);

	Text& Kerning(float kerning);
	Text& Tracking(float tracking);
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

	// Full glow = outer + inner.
	Text& Glow(ptgn::Color color, float width, float softness = 1.0f);
	Text& Glow(ptgn::Color color, float outer_width, float inner_width, float softness);

	Text& ClearSdfEffects();

	Text& Effect(
		GlyphEffectType type, float amplitude, float frequency, float speed, float phase = 0.0f
	);

	const TextLayout& GetLayout() const;
	Rect GetLocalBounds() const;
	std::size_t GetLineCount() const;
	const LineLayout& GetLine(std::size_t index) const;

	bool IsClipped() const;
	bool IsEllipsized() const;
	bool IsTruncatedByMaxLines() const;
	bool IsTruncated() const;

	float GetUsedShrinkScale() const;

	std::size_t GetRunCount() const;
	std::size_t GetCurrentRunIndex() const;

	void InvalidateLayout();

	Text& SetStyledText(StyledText styled_text);

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

	[[nodiscard]] static TextPaginationResult Paginate(
		AssetManager& asset_manager, const StyledText& styled_text, TextBox box,
		const TextPageOptions& options = {}
	);

private:
	const TextLayout& RequireLayout() const;

	StyledText& EnsureStyledText();
	const StyledText& RequireStyledText() const;

	TextBox& EnsureTextBox();
	const TextBox& RequireTextBox() const;

	impl::TextEditState& EnsureEditState();
	const impl::TextEditState& RequireEditState() const;

	void EnsureValidRuns();

	TextRunStyle MakeDefaultRunStyle() const;

	TextRun& CurrentRun();
	const TextRun& CurrentRun() const;

	TextRunStyle& CurrentStyle();
	const TextRunStyle& CurrentStyle() const;

	bool HasOnlyDefaultEmptyRun() const;
};

Text CreateText(Scene& scene, Transform transform = {}, Origin draw_origin = Origin::Center);

PTGN_REGISTER_DRAWABLE(Text);

} // namespace ptgn