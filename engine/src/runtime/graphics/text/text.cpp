#include "runtime/graphics/text/text.h"

#include <ecs/ecs.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "renderer/draw_context.h"
#include "renderer/text/font_atlas.h"
#include "renderer/text/font_style.h"
#include "renderer/text/text_glyph.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

namespace {

void UpdateLayout(
	Text text, AssetManager& asset_manager, const StyledText& styled_text, TextBox box
) {
	auto origin{ text.TryGet<Origin>() };

	Alignment fallback{ origin ? GetAlignment(*origin) : Alignment{} };

	PTGN_ASSERT(fallback.horizontal.has_value(), "Fallback horizontal alignment must have a value");
	PTGN_ASSERT(fallback.vertical.has_value(), "Fallback vertical alignment must have a value");

	if (!box.style.alignment.horizontal.has_value()) {
		box.style.alignment.horizontal = fallback.horizontal.value();
	}

	if (!box.style.alignment.vertical.has_value()) {
		box.style.alignment.vertical = fallback.vertical.value();
	}

	if (auto layout{ text.TryGet<TextLayout>() };
		layout && !layout->dirty && layout->built_alignment == box.style.alignment) {
		return;
	}

	text.Add<TextLayout>(impl::BuildTextLayout(asset_manager, styled_text, box));
}

} // namespace

namespace impl {

ResolvedTextRun ResolveTextRun(AssetManager& asset_manager, const TextRun& text_run) {
	bool has_font{ impl::AssetAccessor{ asset_manager }.Has<Font>(text_run.font) };

	if (!has_font) {
		PTGN_WARN("Font not found: ", text_run.font, ". Using default font instead.");
	}

	FontKey font_key{ has_font ? text_run.font : FontKey{ kDefaultFont } };

	auto font{ impl::AssetAccessor{ asset_manager }.Get<Font>(font_key) };

	auto font_atlas{ &font.GetEntity().Get<impl::FontAtlas>() };

	return {
		.text  = text_run.text,
		.font  = font_atlas,
		.style = text_run.style,
	};
}

ResolvedStyledText ResolveStyledText(AssetManager& asset_manager, const StyledText& styled_text) {
	ResolvedStyledText result;
	result.runs.reserve(styled_text.runs.size());
	for (const auto& run : styled_text.runs) {
		result.runs.emplace_back(ResolveTextRun(asset_manager, run));
	}
	return result;
}

TextLayout BuildTextLayout(
	AssetManager& asset_manager, const StyledText& styled_text, const TextBox& box
) {
	return BuildTextLayout(ResolveStyledText(asset_manager, styled_text), box);
}

} // namespace impl

void Text::Draw(DrawContext& ctx, Entity entity) {
	auto& scene{ entity.GetScene() };

	if (!entity.Has<impl::TextData>()) {
		PTGN_WARN("Text entity cannot be drawn without TextData component");
		return;
	}

	Text text{ entity };

	const auto& data{ entity.Get<impl::TextData>() };

	if (!data.text.HasContent()) {
		return;
	}

	const auto& layout{ text.GetLayout() };

	auto transform{ GetDrawTransform(entity) };
	auto origin{ entity.GetOrDefault<Origin>() };

	auto prepared{ impl::PrepareTextDraw(transform, layout, data.box, origin, data.clip) };

	if (!prepared.drawable) {
		return;
	}

	auto effects{ impl::GetEffectParams(entity) };

	ctx.SetBlendMode(GetBlendMode(entity));

	DrawTextRequest request{
		.layout				= layout,
		.tint				= GetTint(entity),
		.depth				= GetDepth(entity),
		.entity_id			= entity.Get<UUID>(),
		.clips				= prepared.GetClips(),
		.reveal_glyph_count = text.GetRevealGlyphCount(),
		.time				= scene.ctx().TimeSinceStartSeconds().count(),
	};

	ctx.DrawText(prepared.transform, request, effects);
}

Text::Text(Entity entity) : Entity{ entity } {}

Text& Text::Clear() {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot clear text without TextData component");
		return *this;
	}

	auto& data{ Get<impl::TextData>() };

	bool changed{ data.text.HasContent() };

	data.text.runs.clear();
	data.text.runs.emplace_back();

	data.current_run_index = 0;

	if (changed) {
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Content(std::string_view content) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot add text content without TextData component");
		return *this;
	}

	auto& data{ Get<impl::TextData>() };

	if (data.text.runs.size() == 1 && data.text.runs.front().text.empty()) {
		data.current_run_index = 0;

		if (auto& run{ data.text.runs.front() }; run.text != content) {
			run.text = std::string{ content };
			InvalidateLayout();
		}

		return *this;
	}

	auto& run{ data.text.runs.emplace_back() };

	data.current_run_index = data.text.runs.size() - 1;

	if (!content.empty()) {
		run.text = std::string{ content };
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Content(StyledText styled_text) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot add text content without TextData component");
		return *this;
	}

	if (styled_text.runs.empty()) {
		styled_text.runs.emplace_back();
	}

	auto& data{ Get<impl::TextData>() };

	if (data.text != styled_text) {
		data.text = std::move(styled_text);
		InvalidateLayout();
	}

	data.current_run_index = 0;

	return *this;
}

Text& Text::Select(std::size_t index) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot select text index without TextData component");
		return *this;
	}

	auto& data{ Get<impl::TextData>() };

	if (data.text.runs.empty()) {
		data.text.runs.emplace_back();
	}

	data.current_run_index = std::min(index, data.text.runs.size() - 1);

	return *this;
}

Text& Text::Box(Rect text_rect) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text box rect without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; text_rect != box.rect) {
		box.rect = text_rect;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Box(const TextBox& text_box) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text box without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; text_box != box) {
		box = text_box;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Reveal(std::optional<std::size_t> glyph_count) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text glyph reveal count without TextData component");
		return *this;
	}

	auto& data{ Get<impl::TextData>() };
	data.glyph_count = glyph_count;

	return *this;
}

Text& Text::Align(Alignment alignment) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text alignment without TextData component");
		return *this;
	}

	if (auto& current{ Get<impl::TextData>().box.style.alignment }; current != alignment) {
		current = alignment;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Align(Origin origin) {
	return Align(GetAlignment(origin));
}

Text& Text::Align(ptgn::HorizontalAlign horizontal, ptgn::VerticalAlign vertical) {
	return Align(
		{
			.horizontal = horizontal,
			.vertical	= vertical,
		}
	);
}

Text& Text::HorizontalAlign(ptgn::HorizontalAlign horizontal) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text horizontal alignment without TextData component");
		return *this;
	}

	if (auto& current{ Get<impl::TextData>().box.style.alignment }; current.horizontal != horizontal) {
		current.horizontal = horizontal;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::VerticalAlign(ptgn::VerticalAlign vertical) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text vertical alignment without TextData component");
		return *this;
	}

	if (auto& current{ Get<impl::TextData>().box.style.alignment }; current.vertical != vertical) {
		current.vertical = vertical;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::ClearAlignment() {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot clear text alignment without TextData component");
		return *this;
	}

	if (auto& current{ Get<impl::TextData>().box.style.alignment }; current.vertical.has_value() || current.horizontal.has_value()) {
		current.vertical.reset();
		current.horizontal.reset();
		InvalidateLayout();
	}

	return *this;
}

V2_float Text::GetSize() const {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot get text size without TextData component");
		return {};
	}

	return GetLayout().size;
}

Rect Text::GetBounds() const {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot get text bounds without TextData component");
		return {};
	}

	const auto& layout{ GetLayout() };
	auto bounds{ layout.GetBounds() };

	if (const auto& box{ Get<impl::TextData>().box }; box.HasBox()) {
		auto origin{ GetOrDefault<Origin>() };
		auto origin_point{ box.rect.GetOriginPoint(origin) };
		// TODO: Check if this is correct.
		return bounds.Translated(-origin_point);
	}

	return bounds;
}

Text& Text::Wrap(WrapMode mode) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text wrap without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; box.style.wrap.mode != mode) {
		box.style.wrap.mode = mode;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::WrapSettings(const ptgn::WrapSettings& settings) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text wrap settings without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; box.style.wrap != settings) {
		box.style.wrap = settings;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Overflow(OverflowMode mode) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text overflow without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; box.style.overflow != mode) {
		box.style.overflow = mode;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Clip(std::optional<TextClip> clip) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text clip without TextData component");
		return *this;
	}

	auto& data{ Get<impl::TextData>() };
	data.clip = clip;

	return *this;
}

Text& Text::CollapseSpaces(bool collapse) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot collapse text spaces without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; box.style.collapse_spaces != collapse) {
		box.style.collapse_spaces = collapse;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::JustifyLastLine(bool justify) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot just last text line without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; box.style.justify_last_line != justify) {
		box.style.justify_last_line = justify;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::TabWidth(std::size_t spaces) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text tab width without TextData component");
		return *this;
	}

	spaces = std::max(1uz, spaces);

	if (auto& box{ Get<impl::TextData>().box }; box.style.tab_width != spaces) {
		box.style.tab_width = spaces;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::MaxLines(std::size_t max_lines) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set max text lines without TextData component");
		return *this;
	}

	if (auto& box{ Get<impl::TextData>().box }; box.style.max_lines != max_lines) {
		box.style.max_lines = max_lines;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::ScaleToFit(float min_scale, float max_scale) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text scale to fit without TextData component");
		return *this;
	}

	PTGN_ASSERT(
		std::isfinite(min_scale) && std::isfinite(max_scale),
		"Text scale limits must be finite"
	);

	// Order is important.
	max_scale = std::max(kEpsilon<float>, max_scale);
	min_scale = std::clamp(min_scale, kEpsilon<float>, max_scale);

	ShrinkScale scale{ .min = min_scale, .max = max_scale };

	if (auto& style{ Get<impl::TextData>().box.style };
		style.overflow != OverflowMode::ScaleToFit || style.shrink_scale != scale) {
		style.overflow		   = OverflowMode::ScaleToFit;
		style.shrink_scale.min = min_scale;
		style.shrink_scale.max = max_scale;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Font(FontKey font_key) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text font without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; run.font != font_key) {
		run.font = std::move(font_key);
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Color(ptgn::Color color) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text color without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; run.style.color != color) {
		run.style.color = color;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Size(float font_size) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text size without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; !NearlyEqual(run.style.size, font_size)) {
		run.style.size = font_size;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Kerning(float kerning) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text kerning without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; !NearlyEqual(run.style.kerning, kerning)) {
		run.style.kerning = kerning;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Tracking(float tracking) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text tracking without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; !NearlyEqual(run.style.tracking, tracking)) {
		run.style.tracking = tracking;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::LineSpacing(float line_spacing) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text line spacing without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; !NearlyEqual(run.style.line_spacing, line_spacing)) {
		run.style.line_spacing = line_spacing;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Style(FontStyle flags) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text font style without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; run.style.flags != flags) {
		run.style.flags = flags;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Bold(bool enabled, float weight) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot bold text without TextData component");
		return *this;
	}

	auto& run{ CurrentRun() };

	bool was_enabled{ HasFontFlag(run.style.flags, FontStyle::Bold) };
	bool layout_changed{ was_enabled != enabled ||
						 (enabled && !NearlyEqual(run.style.bold_weight, weight)) };

	run.style.flags		  = SetFontFlag(run.style.flags, FontStyle::Bold, enabled);
	run.style.bold_weight = weight;

	if (layout_changed) {
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Italic(bool enabled) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot italicize text without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; HasFontFlag(run.style.flags, FontStyle::Italic) != enabled) {
		run.style.flags = SetFontFlag(run.style.flags, FontStyle::Italic, enabled);
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Underline(bool enabled) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot underline text without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; HasFontFlag(run.style.flags, FontStyle::Underline) != enabled) {
		run.style.flags = SetFontFlag(run.style.flags, FontStyle::Underline, enabled);
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Strikethrough(bool enabled) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot strikethrough text without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() };
		HasFontFlag(run.style.flags, FontStyle::Strikethrough) != enabled) {
		run.style.flags = SetFontFlag(run.style.flags, FontStyle::Strikethrough, enabled);
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Outline(ptgn::Color color, float width, float softness) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot outline text without TextData component");
		return *this;
	}

	DistanceFieldLayerStyle outline{ .color = color, .width = width, .softness = softness };
	if (auto& run{ CurrentRun() }; run.style.sdf.outline != outline) {
		run.style.sdf.outline = outline;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Shadow(ptgn::Color color, V2_float offset, float width, float softness) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text shadow without TextData component");
		return *this;
	}

	DistanceFieldLayerStyle shadow{ .color = color, .width = width, .softness = softness };
	if (auto& run{ CurrentRun() };
		run.style.sdf.shadow != shadow || run.style.sdf.shadow_offset != offset) {
		run.style.sdf.shadow		= shadow;
		run.style.sdf.shadow_offset = offset;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Shadow(ptgn::Color color, V2_float offset, float softness) {
	return Shadow(color, offset, 0.0f, softness);
}

Text& Text::OuterGlow(ptgn::Color color, float width, float softness) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set outer text glow without TextData component");
		return *this;
	}

	DistanceFieldLayerStyle outer_glow{ .color = color, .width = width, .softness = softness };
	if (auto& run{ CurrentRun() }; run.style.sdf.outer_glow != outer_glow) {
		run.style.sdf.outer_glow = outer_glow;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::InnerGlow(ptgn::Color color, float width, float softness) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set inner text glow without TextData component");
		return *this;
	}

	DistanceFieldLayerStyle inner_glow{ .color = color, .width = width, .softness = softness };
	if (auto& run{ CurrentRun() }; run.style.sdf.inner_glow != inner_glow) {
		run.style.sdf.inner_glow = inner_glow;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::ClearSdfEffects() {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot clear text sdf effects without TextData component");
		return *this;
	}

	if (auto& run{ CurrentRun() }; run.style.sdf != DistanceFieldStyle{}) {
		run.style.sdf = {};
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Effect(
	GlyphEffectType type, float amplitude, float frequency, float speed, float phase
) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text effect without TextData component");
		return *this;
	}

	GlyphEffectStyle effect{
		.type = type, .amplitude = amplitude, .frequency = frequency, .speed = speed, .phase = phase
	};
	if (auto& run{ CurrentRun() }; run.style.effect != effect) {
		run.style.effect = effect;
		InvalidateLayout();
	}

	return *this;
}

TextMeasurement Text::Measure() const {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot measure text without TextData component");
		return {};
	}
	
	const auto& layout{ GetLayout() };

	return {
		.size			   = layout.size,
		.line_count		   = layout.lines.size(),
		.truncated		   = layout.truncated,
		.used_shrink_scale = layout.used_shrink_scale,
	};
}

Text& Text::RevealFraction(float fraction) {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot set text reveal fraction without TextData component");
		return *this;
	}

	fraction = std::clamp(fraction, 0.0f, 1.0f);

	auto glyph_count{ GetLayout().GetVisibleGlyphCount() };

	auto reveal_count{
		static_cast<std::size_t>(std::round(static_cast<float>(glyph_count) * fraction))
	};

	return Reveal(reveal_count);
}

std::size_t Text::GetRevealGlyphCount() const {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot get text glyph reveal count without TextData component");
		return 0;
	}

	const auto& data{ Get<impl::TextData>() };

	return data.glyph_count.value_or(std::numeric_limits<std::size_t>::max());
}

bool Text::IsFullyRevealed() const {
	if (!Has<impl::TextData>()) {
		PTGN_WARN("Cannot check if text is fully revealed without TextData component");
		return false;
	}

	auto glyph_count{ GetLayout().GetVisibleGlyphCount() };
	auto revealed_glyphs{ GetRevealGlyphCount() };

	return revealed_glyphs >= glyph_count;
}

const StyledText& Text::GetStyledText() const {
	return Get<impl::TextData>().text;
}

const TextBox& Text::GetTextBox() const {
	return Get<impl::TextData>().box;
}

const TextLayout& Text::GetLayout() const {
	UpdateLayout();
	return Get<TextLayout>();
}

void Text::UpdateLayout() const {
	auto& asset_manager{ GetScene().ctx().asset };
	const auto& styled_text{ GetStyledText() };
	const auto& box{ GetTextBox() };

	ptgn::UpdateLayout(*this, asset_manager, styled_text, box);
}

TextRun& Text::CurrentRun() {
	PTGN_ASSERT(
		Has<impl::TextData>(),
		"Cannot get current text run without TextData component"
	);

	auto& data{ Get<impl::TextData>() };

	if (data.text.runs.empty()) {
		data.text.runs.emplace_back();
		data.current_run_index = 0;
	}

	data.current_run_index =
		std::min(data.current_run_index, data.text.runs.size() - 1);

	return data.text.runs[data.current_run_index];
}

void Text::InvalidateLayout() {
	if (auto layout{ TryGet<TextLayout>() }) {
		layout->dirty = true;
	}
}

Text CreateText(Scene& scene, Transform transform, StyledText styled_text, Origin origin) {
	Text text{ scene.CreateEntity() };

	text.Add<impl::TextData>();
	text.Add<TextLayout>();
	text.Add<Transform>(transform);
	text.Add<Origin>(origin);
	text.Add<Visible>(true);
	text.Add<Tag>("Text");

	text.Content(std::move(styled_text));

	SetDraw<Text>(text);

	return text;
}

Text CreateText(
	Scene& scene, Transform transform, std::string_view content, Color color, float font_size,
	Origin origin, FontKey font
) {
	return CreateText(
		scene, transform,
		{ { .text  = std::string{ content },
			.font  = std::move(font),
			.style = { .color = color, .size = font_size } } },
		origin
	);
}

} // namespace ptgn