#include "runtime/graphics/text/text.h"

#include <ecs/ecs.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
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
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/render_queue.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

namespace {

void UpdateLayout(
	Entity entity, AssetManager& asset_manager, const StyledText& styled_text, const TextBox& box
) {
	if (auto layout{ entity.TryGet<TextLayout>() }; layout && !layout->dirty) {
		return;
	}

	entity.Add<TextLayout>(impl::BuildTextLayout(asset_manager, styled_text, box));
}

} // namespace

namespace impl {

ResolvedTextRun ResolveTextRun(AssetManager& asset_manager, const TextRun& text_run) {
	auto font{ impl::AssetAccessor{ asset_manager }.Get<Font>(text_run.font) };
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

void DrawDebugTextBoundingBoxes(
	Scene& scene, const std::optional<SceneCamera>& camera, const impl::EntityFilterFunc& filter
) {
	TextDebugSettings settings{ scene.ctx().debug.text };

	if (scene.ctx().debug_local.text.draw_enabled) {
		settings = scene.ctx().debug_local.text;
	}

	if (!settings.draw_enabled) {
		return;
	}

	for (const auto& [entity, styled_text, box] :
		 std::as_const(scene).EntitiesWith<StyledText, TextBox>()) {
		if (filter(entity)) {
			continue;
		}

		if (!styled_text.HasContent()) {
			continue;
		}

		const auto& layout{ Text{ entity }.GetLayout() };

		auto transform{ GetDrawTransform(entity) };
		auto origin{ GetDrawOrigin(entity) };

		auto prepared{ impl::PrepareTextDraw(transform, layout, box, origin) };

		if (!prepared.drawable) {
			continue;
		}

		auto shape_params = [&] {
			return ShapeRenderParams{
				.fill_style = settings.draw_line_width,
				.origin		= Origin::Center,
				.camera		= camera,
				.debug		= true,
			};
		};

		auto draw_rect = [&](Rect rect, Color color) {
			if (!rect.GetSize().IsPositive()) {
				return;
			}

			// TODO: Dont translate.
			auto transform{ prepared.transform };
			transform.Translate(rect.GetCenter());

			scene.ctx().render_queue.DrawShape(
				transform, Rect{ rect.GetSize() }, color, shape_params()
			);
		};

		auto draw_line = [&](V2_float start, V2_float end, Color color) {
			scene.ctx().render_queue.DrawLine(
				start, end, color, shape_params(), prepared.transform
			);
		};

		auto layout_bounds{ layout.GetBounds() };

		if (box.HasArea()) {
			// Both dimensions are constrained, so draw the complete text box.
			draw_rect(box.rect, settings.draw_color);
		} else if (box.HasWidth()) {
			// Only width is constrained. Draw the two vertical boundaries
			// where the left and right sides of the text box would be.
			draw_line(
				{ box.rect.min.x, layout_bounds.min.y }, { box.rect.min.x, layout_bounds.max.y },
				settings.draw_color
			);

			draw_line(
				{ box.rect.max.x, layout_bounds.min.y }, { box.rect.max.x, layout_bounds.max.y },
				settings.draw_color
			);
		} else if (box.HasHeight()) {
			// Only height is constrained. Draw the two horizontal boundaries
			// where the top and bottom sides of the text box would be.
			draw_line(
				{ layout_bounds.min.x, box.rect.min.y }, { layout_bounds.max.x, box.rect.min.y },
				settings.draw_color
			);

			draw_line(
				{ layout_bounds.min.x, box.rect.max.y }, { layout_bounds.max.x, box.rect.max.y },
				settings.draw_color
			);
		} else {
			// Unconstrained text has no explicit box, so show its generated
			// logical bounds instead.
			draw_rect(layout_bounds, settings.draw_color);
		}

		// An explicit clip rectangle always has both dimensions and is local
		// to the same prepared text transform.
		if (auto clip{ entity.TryGet<impl::TextClip>() }; clip && clip->rect.has_value()) {
			draw_rect(clip->rect.value(), settings.clip_draw_color);
		}
	}
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

	auto origin{ GetDrawOrigin(entity) };

	auto prepared{ impl::PrepareTextDraw(transform, layout, box, origin, explicit_clip) };

	if (!prepared.drawable) {
		return;
	}

	auto effects{ impl::GetEffectParams(entity) };

	ctx.SetBlendMode(GetBlendMode(entity));

	ctx.DrawText(
		prepared.transform,
		DrawTextRequest{
			.layout				= layout,
			.tint				= GetTint(entity),
			.depth				= GetDepth(entity),
			.entity_id			= entity.GetUUID(),
			.clips				= prepared.GetClips(),
			.reveal_glyph_count = text.GetRevealGlyphCount(),
			.time				= scene.ctx().TimeSinceStartSeconds().count(),
		},
		effects
	);
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
	if (auto& box{ Get<TextBox>() }; box.style.alignment != alignment) {
		box.style.alignment = alignment;
		InvalidateLayout();
	}

	auto& override{ TryAdd<impl::TextAlignmentOverride>() };
	override.horizontal = true;
	override.vertical	= true;

	return *this;
}

Text& Text::Align(ptgn::HorizontalAlign horizontal, ptgn::VerticalAlign vertical) {
	return Align({ .horizontal = horizontal, .vertical = vertical });
}

Text& Text::HorizontalAlign(ptgn::HorizontalAlign align) {
	if (auto& box{ Get<TextBox>() }; box.style.alignment.horizontal != align) {
		box.style.alignment.horizontal = align;
		InvalidateLayout();
	}

	TryAdd<impl::TextAlignmentOverride>().horizontal = true;

	return *this;
}

Text& Text::VerticalAlign(ptgn::VerticalAlign align) {
	if (auto& box{ Get<TextBox>() }; box.style.alignment.vertical != align) {
		box.style.alignment.vertical = align;
		InvalidateLayout();
	}

	TryAdd<impl::TextAlignmentOverride>().vertical = true;

	return *this;
}

Text& Text::ClearAlignment() {
	auto origin{ GetDrawOrigin(*this) };
	auto alignment{ GetAlignment(origin) };

	if (auto& box{ Get<TextBox>() }; box.style.alignment != alignment) {
		box.style.alignment = alignment;
		InvalidateLayout();
	}

	Remove<impl::TextAlignmentOverride>();

	return *this;
}

V2_float Text::GetSize() const {
	return GetLayout().size;
}

Rect Text::GetBounds() const {
	const auto& layout{ GetLayout() };
	auto bounds{ layout.GetBounds() };

	if (const auto& box{ GetTextBox() }; box.HasBox()) {
		auto origin{ GetDrawOrigin(*this) };
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

Text& Text::Font(std::string_view font_key) {
	if (auto& run{ CurrentRun() }; run.font != font_key) {
		run.font = std::string{ font_key };
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

void Text::OverrideAlignment(Alignment alignment) {
	auto alignment_override{ TryGet<impl::TextAlignmentOverride>() };
	auto& current{ Get<TextBox>().style.alignment };

	bool changed{ false };

	if ((!alignment_override || !alignment_override->horizontal) &&
		current.horizontal != alignment.horizontal) {
		current.horizontal = alignment.horizontal;
		changed			   = true;
	}

	if ((!alignment_override || !alignment_override->vertical) &&
		current.vertical != alignment.vertical) {
		current.vertical = alignment.vertical;
		changed			 = true;
	}

	if (changed) {
		InvalidateLayout();
	}
}

Text CreateText(Scene& scene, Transform transform, StyledText styled_text, Origin origin) {
	Text text{ scene.CreateEntity() };

	text.Add<impl::TextEditState>();
	text.Add<StyledText>();
	text.Add<TextBox>();
	text.Add<TextLayout>();

	text.Content(std::move(styled_text));
	text.Box(TextBox{ .style = { .alignment{ GetAlignment(origin) } } });

	SetTransform(text, transform);
	SetDrawOrigin(text, origin);
	SetDraw<Text>(text);
	Show(text, true);

	return text;
}

Text CreateText(
	Scene& scene, Transform transform, std::string_view content, Color color, float font_size,
	Origin origin, std::string_view font
) {
	return CreateText(
		scene, transform,
		{ { .text  = std::string{ content },
			.font  = std::string{ font },
			.style = { .color = color, .size = font_size } } },
		origin
	);
}

} // namespace ptgn