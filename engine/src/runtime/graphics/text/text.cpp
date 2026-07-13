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

	auto font_key{ has_font ? text_run.font : kDefaultFont };

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

	if (!entity.Has<StyledText, TextBox>()) {
		return;
	}

	Text text{ entity };

	if (const auto& styled_text{ text.GetStyledText() }; !styled_text.HasContent()) {
		return;
	}

	const auto& box{ text.GetTextBox() };

	const auto& layout{ text.GetLayout() };

	std::optional<TextClipConstraint> explicit_clip;

	if (auto clip{ entity.TryGet<impl::TextClip>() }; clip && clip->rect.has_value()) {
		explicit_clip = TextClipConstraint{
			.rect = clip->rect.value(),
			.mode = clip->mode == TextClipMode::None ? TextClipMode::Clip : clip->mode,
		};
	}

	auto transform{ GetDrawTransform(entity) };

	auto origin{ entity.GetOrDefault<Origin>() };

	auto prepared{ impl::PrepareTextDraw(transform, layout, box, origin, explicit_clip) };

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
	auto& styled_text{ Get<StyledText>() };
	auto& edit_state{ Get<impl::TextEditState>() };

	bool changed{ styled_text.HasContent() };

	styled_text.runs.clear();
	styled_text.runs.emplace_back();

	edit_state.current_run_index = 0;

	if (changed) {
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Content(std::string_view content) {
	auto& styled_text{ Get<StyledText>() };
	auto& edit_state{ Get<impl::TextEditState>() };

	if (styled_text.runs.size() == 1 && styled_text.runs.front().text.empty()) {
		edit_state.current_run_index = 0;

		if (auto& run{ styled_text.runs.front() }; run.text != content) {
			run.text = std::string{ content };
			InvalidateLayout();
		}

		return *this;
	}

	auto& run{ styled_text.runs.emplace_back() };

	edit_state.current_run_index = styled_text.runs.size() - 1;

	if (!content.empty()) {
		run.text = std::string{ content };
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Content(StyledText styled_text) {
	if (styled_text.runs.empty()) {
		styled_text.runs.emplace_back();
	}

	if (auto& text{ Get<StyledText>() }; text != styled_text) {
		text = std::move(styled_text);
		InvalidateLayout();
	}

	Get<impl::TextEditState>().current_run_index = 0;

	return *this;
}

Text& Text::Select(std::size_t index) {
	if (const auto& styled_text{ GetStyledText() }; index >= styled_text.runs.size()) {
		index = styled_text.runs.size() - 1;
	}

	Get<impl::TextEditState>().current_run_index = index;

	return *this;
}

Text& Text::Box(Rect text_rect) {
	if (auto& box{ Get<TextBox>() }; text_rect != box.rect) {
		box.rect = text_rect;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Box(const TextBox& text_box) {
	if (auto& box{ Get<TextBox>() }; text_box != box) {
		box = text_box;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Reveal(std::size_t glyph_count) {
	auto& reveal{ TryAdd<impl::TextReveal>() };
	reveal.glyph_count = glyph_count;
	return *this;
}

Text& Text::RevealAll() {
	Remove<impl::TextReveal>();
	return *this;
}

Text& Text::Align(Alignment alignment) {
	auto& current{ Get<TextBox>().style.alignment };

	if (current != alignment) {
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
	auto& alignment{ Get<TextBox>().style.alignment };

	if (alignment.horizontal != horizontal) {
		alignment.horizontal = horizontal;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::VerticalAlign(ptgn::VerticalAlign vertical) {
	auto& alignment{ Get<TextBox>().style.alignment };

	if (alignment.vertical != vertical) {
		alignment.vertical = vertical;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::ClearAlignment() {
	auto& alignment{ Get<TextBox>().style.alignment };

	if (alignment.vertical.has_value() || alignment.horizontal.has_value()) {
		alignment.vertical.reset();
		alignment.horizontal.reset();
		InvalidateLayout();
	}

	return *this;
}

V2_float Text::GetSize() const {
	return GetLayout().size;
}

Rect Text::GetBounds() const {
	const auto& layout{ GetLayout() };
	auto bounds{ layout.GetBounds() };

	if (const auto& box{ GetTextBox() }; box.HasBox()) {
		auto origin{ GetOrDefault<Origin>() };
		auto origin_point{ box.rect.GetOriginPoint(origin) };
		// TODO: Check if this is correct.
		return bounds.Translated(-origin_point);
	}

	return bounds;
}

Text& Text::Wrap(WrapMode mode) {
	if (auto& box{ Get<TextBox>() }; box.style.wrap.mode != mode) {
		box.style.wrap.mode = mode;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::WrapSettings(const ptgn::WrapSettings& settings) {
	if (auto& box{ Get<TextBox>() }; box.style.wrap != settings) {
		box.style.wrap = settings;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Overflow(OverflowMode mode) {
	if (auto& box{ Get<TextBox>() }; box.style.overflow != mode) {
		box.style.overflow = mode;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Clip(Rect rect, TextClipMode mode) {
	Add<impl::TextClip>(impl::TextClip{
		.rect = rect,
		.mode = mode,
	});
	return *this;
}

Text& Text::ClearClip() {
	Remove<impl::TextClip>();
	return *this;
}

Text& Text::CollapseSpaces(bool collapse) {
	if (auto& box{ Get<TextBox>() }; box.style.collapse_spaces != collapse) {
		box.style.collapse_spaces = collapse;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::JustifyLastLine(bool justify) {
	if (auto& box{ Get<TextBox>() }; box.style.justify_last_line != justify) {
		box.style.justify_last_line = justify;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::TabWidth(std::size_t spaces) {
	PTGN_ASSERT(spaces > 0, "Text tab width must be at least one space");
	if (auto& box{ Get<TextBox>() }; box.style.tab_width != spaces) {
		box.style.tab_width = spaces;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::MaxLines(std::size_t max_lines) {
	if (auto& box{ Get<TextBox>() }; box.style.max_lines != max_lines) {
		box.style.max_lines = max_lines;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::ScaleToFit(float min_scale, float max_scale) {
	PTGN_ASSERT(min_scale > 0.0f, "Minimum text scale must be positive");
	PTGN_ASSERT(max_scale > 0.0f, "Maximum text scale must be positive");
	PTGN_ASSERT(min_scale <= max_scale, "Minimum text scale cannot exceed maximum text scale");

	ShrinkScale scale{ .min = min_scale, .max = max_scale };

	if (auto& style{ Get<TextBox>().style };
		style.overflow != OverflowMode::ScaleToFit || style.shrink_scale != scale) {
		style.overflow		   = OverflowMode::ScaleToFit;
		style.shrink_scale.min = min_scale;
		style.shrink_scale.max = max_scale;
		InvalidateLayout();
	}

	return *this;
}

Text& Text::Font(FontKey font_key) {
	if (auto& run{ CurrentRun() }; run.font != font_key) {
		run.font = std::move(font_key);
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Color(ptgn::Color color) {
	if (auto& run{ CurrentRun() }; run.style.color != color) {
		run.style.color = color;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Size(float font_size) {
	if (auto& run{ CurrentRun() }; !NearlyEqual(run.style.size, font_size)) {
		run.style.size = font_size;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Kerning(float kerning) {
	if (auto& run{ CurrentRun() }; !NearlyEqual(run.style.kerning, kerning)) {
		run.style.kerning = kerning;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Tracking(float tracking) {
	if (auto& run{ CurrentRun() }; !NearlyEqual(run.style.tracking, tracking)) {
		run.style.tracking = tracking;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::LineSpacing(float line_spacing) {
	if (auto& run{ CurrentRun() }; !NearlyEqual(run.style.line_spacing, line_spacing)) {
		run.style.line_spacing = line_spacing;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Style(FontStyle flags) {
	if (auto& run{ CurrentRun() }; run.style.flags != flags) {
		run.style.flags = flags;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Bold(bool enabled, float weight) {
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
	if (auto& run{ CurrentRun() }; HasFontFlag(run.style.flags, FontStyle::Italic) != enabled) {
		run.style.flags = SetFontFlag(run.style.flags, FontStyle::Italic, enabled);
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Underline(bool enabled) {
	if (auto& run{ CurrentRun() }; HasFontFlag(run.style.flags, FontStyle::Underline) != enabled) {
		run.style.flags = SetFontFlag(run.style.flags, FontStyle::Underline, enabled);
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Strikethrough(bool enabled) {
	if (auto& run{ CurrentRun() };
		HasFontFlag(run.style.flags, FontStyle::Strikethrough) != enabled) {
		run.style.flags = SetFontFlag(run.style.flags, FontStyle::Strikethrough, enabled);
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Outline(ptgn::Color color, float width, float softness) {
	DistanceFieldLayerStyle outline{ .color = color, .width = width, .softness = softness };
	if (auto& run{ CurrentRun() }; run.style.sdf.outline != outline) {
		run.style.sdf.outline = outline;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Shadow(ptgn::Color color, V2_float offset, float softness) {
	return Shadow(color, offset, 0.0f, softness);
}

Text& Text::Shadow(ptgn::Color color, V2_float offset, float width, float softness) {
	DistanceFieldLayerStyle shadow{ .color = color, .width = width, .softness = softness };
	if (auto& run{ CurrentRun() };
		run.style.sdf.shadow != shadow || run.style.sdf.shadow_offset != offset) {
		run.style.sdf.shadow		= shadow;
		run.style.sdf.shadow_offset = offset;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::OuterGlow(ptgn::Color color, float width, float softness) {
	DistanceFieldLayerStyle outer_glow{ .color = color, .width = width, .softness = softness };
	if (auto& run{ CurrentRun() }; run.style.sdf.outer_glow != outer_glow) {
		run.style.sdf.outer_glow = outer_glow;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::InnerGlow(ptgn::Color color, float width, float softness) {
	DistanceFieldLayerStyle inner_glow{ .color = color, .width = width, .softness = softness };
	if (auto& run{ CurrentRun() }; run.style.sdf.inner_glow != inner_glow) {
		run.style.sdf.inner_glow = inner_glow;
		InvalidateLayout();
	}
	return *this;
}

Text& Text::ClearSdfEffects() {
	if (auto& run{ CurrentRun() }; run.style.sdf != DistanceFieldStyle{}) {
		run.style.sdf = {};
		InvalidateLayout();
	}
	return *this;
}

Text& Text::Effect(
	GlyphEffectType type, float amplitude, float frequency, float speed, float phase
) {
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
	const auto& layout{ GetLayout() };
	return {
		.size			   = layout.size,
		.line_count		   = layout.lines.size(),
		.truncated		   = layout.truncated,
		.used_shrink_scale = layout.used_shrink_scale,
	};
}

Text& Text::RevealFraction(float fraction) {
	fraction = std::clamp(fraction, 0.0f, 1.0f);

	auto glyph_count{ GetLayout().GetVisibleGlyphCount() };

	auto reveal_count{
		static_cast<std::size_t>(std::round(static_cast<float>(glyph_count) * fraction))
	};

	return Reveal(reveal_count);
}

std::size_t Text::GetRevealGlyphCount() const {
	if (auto reveal{ TryGet<impl::TextReveal>() }) {
		return reveal->glyph_count;
	}
	return std::numeric_limits<std::size_t>::max();
}

bool Text::IsFullyRevealed() const {
	if (auto reveal{ TryGet<impl::TextReveal>() }) {
		auto glyph_count{ GetLayout().GetVisibleGlyphCount() };
		return reveal->glyph_count >= glyph_count;
	}
	return true;
}

const StyledText& Text::GetStyledText() const {
	return Get<StyledText>();
}

const TextBox& Text::GetTextBox() const {
	return Get<TextBox>();
}

const TextLayout& Text::GetLayout() const {
	auto& asset_manager{ GetScene().ctx().asset };
	const auto& styled_text{ GetStyledText() };
	const auto& box{ GetTextBox() };

	UpdateLayout(*this, asset_manager, styled_text, box);

	return Get<TextLayout>();
}

TextRun& Text::CurrentRun() {
	auto& styled_text{ Get<StyledText>() };
	const auto& edit_state{ Get<impl::TextEditState>() };

	PTGN_ASSERT(
		edit_state.current_run_index < styled_text.runs.size(), "Invalid current text run index"
	);

	return styled_text.runs[edit_state.current_run_index];
}

void Text::InvalidateLayout() {
	if (auto layout{ TryGet<TextLayout>() }) {
		layout->dirty = true;
	}
}

Text CreateText(Scene& scene, Transform transform, StyledText styled_text, Origin origin) {
	Text text{ scene.CreateEntity() };

	text.Add<impl::TextEditState>();
	text.Add<StyledText>();
	text.Add<TextBox>();
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