#pragma once

#include <string_view>

#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/text/font_style.h"
#include "runtime/graphics/text/text_effect.h"
#include "runtime/graphics/text/text_layout.h"
#include "runtime/graphics/text/text_style.h"

namespace ptgn {

class DrawContext;
class Scene;
class AssetManager;

namespace impl {

struct TextEditState {
	std::size_t current_run_index{ 0 };
};

void DrawTextLayoutDebug(Scene& scene);

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

	std::size_t GetRunCount() const;
	std::size_t GetCurrentRunIndex() const;

	void InvalidateLayout();

	Text& SetStyledText(StyledText styled_text);

	[[nodiscard]] TextMeasurement Measure() const;
	[[nodiscard]] std::size_t GetGlyphCount() const;
	[[nodiscard]] std::size_t GetVisibleGlyphCount() const;
	[[nodiscard]] bool IsFullyRevealed() const;

	Text& RevealFraction(float fraction);

	[[nodiscard]] static TextPaginationResult Paginate(
		AssetManager& asset_manager, const StyledText& styled_text, TextBox box,
		const TextPageOptions& options = {}
	);

private:
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

Text CreateText(Scene& scene, V2_float position = {}, Origin draw_origin = Origin::Center);

PTGN_REGISTER_DRAWABLE(Text);

} // namespace ptgn