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
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "core/util/hash.h"
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

FontStyle SetFlag(FontStyle value, FontStyle flag, bool enabled) {
	auto bits{ std::to_underlying(value) };
	auto flag_bits{ std::to_underlying(flag) };

	if (enabled) {
		bits |= flag_bits;
	} else {
		bits &= ~flag_bits;
	}

	return static_cast<FontStyle>(bits);
}

bool HasVisibleTextContent(const StyledText& styled_text) {
	return std::ranges::any_of(styled_text.runs, [](const auto& run) { return !run.text.empty(); });
}

impl::ResolvedStyledText ResolveStyledText(AssetManager& asset_manager, const TextRun& text_run) {
	impl::ResolvedStyledText styled_text;
	styled_text.runs.emplace_back(impl::ResolveTextRun(asset_manager, text_run));
	return styled_text;
}

bool FitsTextPage(
	AssetManager& asset_manager, const TextRun& text_run, TextBox box, std::size_t max_lines
) {
	auto styled_text{ ResolveStyledText(asset_manager, text_run) };

	box.style.overflow_mode = OverflowMode::Overflow;

	auto layout{ impl::BuildTextLayout(styled_text, box) };

	if (max_lines > 0 && layout.lines.size() > max_lines) {
		return false;
	}

	return impl::TextLayoutFitsInBox(layout, box.rect);
}

void UpdateLayout(
	Entity entity, AssetManager& asset_manager, const StyledText& styled_text, const TextBox& box
) {
	auto hash{ Hash(styled_text, box) };

	if (auto cached{ entity.TryGet<TextLayout>() }; cached && cached->hash == hash) {
		return;
	}

	auto layout{ impl::BuildTextLayout(asset_manager, styled_text, box) };
	layout.hash = hash;

	entity.Add<TextLayout>(std::move(layout));
}

std::optional<Rect> IntersectClipRects(std::optional<Rect> a, std::optional<Rect> b) {
	if (!a.has_value()) {
		return b;
	}

	if (!b.has_value()) {
		return a;
	}

	Rect result{
		{
			std::max(a.value().min.x, b.value().min.x),
			std::max(a.value().min.y, b.value().min.y),
		},
		{
			std::min(a.value().max.x, b.value().max.x),
			std::min(a.value().max.y, b.value().max.y),
		},
	};

	if (!result.GetSize().IsPositive()) {
		return Rect{};
	}

	return result;
}

std::pair<HorizontalAlign, VerticalAlign> GetTextAlignment(Origin origin) {
	switch (origin) {
		using enum Origin;
		case TopLeft:	   return { HorizontalAlign::Left, VerticalAlign::Top };
		case CenterTop:	   return { HorizontalAlign::Center, VerticalAlign::Top };
		case TopRight:	   return { HorizontalAlign::Right, VerticalAlign::Top };
		case CenterRight:  return { HorizontalAlign::Right, VerticalAlign::Center };
		case BottomRight:  return { HorizontalAlign::Right, VerticalAlign::Bottom };
		case CenterBottom: return { HorizontalAlign::Center, VerticalAlign::Bottom };
		case BottomLeft:   return { HorizontalAlign::Left, VerticalAlign::Bottom };
		case CenterLeft:   return { HorizontalAlign::Left, VerticalAlign::Center };
		case Center:	   return { HorizontalAlign::Center, VerticalAlign::Center };
		default:		   PTGN_ERROR("Unknown Origin: ", std::to_underlying(origin));
	}
}

V2_float GetTextOriginPoint(Entity entity, const TextLayout& layout, const TextBox& box) {
	if (box.HasBox()) {
		return box.rect.GetOriginPoint(GetDrawOrigin(entity));
	}

	auto origin_point{ layout.GetBounds().GetOriginPoint(GetDrawOrigin(entity)) };

	// In unboxed text, an explicitly selected alignment anchors that axis directly to the
	// transform. The draw origin remains the fallback anchor for axes the user did not override.
	if (auto alignment_override{ entity.TryGet<impl::TextAlignmentOverride>() }) {
		if (alignment_override->horizontal) {
			origin_point.x = 0.0f;
		}
		if (alignment_override->vertical) {
			origin_point.y = 0.0f;
		}
	}

	return origin_point;
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

	for (auto [entity, styled_text, box] : scene.EntitiesWith<StyledText, TextBox>()) {
		if (filter(entity) || !HasVisibleTextContent(styled_text)) {
			continue;
		}

		UpdateLayout(entity, scene.ctx().asset, styled_text, box);

		const auto* layout{ entity.TryGet<TextLayout>() };

		if (!layout) {
			continue;
		}

		auto prepared{
			impl::PrepareTextDraw(GetDrawTransform(entity), box, GetDrawOrigin(entity))
		};

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
			if (!rect.HasPositiveArea()) {
				return;
			}

			auto transform{ prepared.transform };
			transform.Translate(rect.GetCenter());

			scene.ctx().render_queue.DrawShape(
				transform, Rect{ rect.GetSize() }, color, shape_params()
			);
		};

		auto draw_line = [&](V2_float start, V2_float end, Color color) {
			std::array<V2_float, 2> points{
				start,
				end,
			};

			scene.ctx().render_queue.DrawLines(
				points, color, shape_params(), false, prepared.transform
			);
		};

		auto layout_bounds{ layout->GetBounds() };

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

	const auto& styled_text{ entity.Get<StyledText>() };
	const auto& box{ entity.Get<TextBox>() };

	if (!HasVisibleTextContent(styled_text)) {
		return;
	}

	UpdateLayout(entity, scene.ctx().asset, styled_text, box);

	const auto& layout{ entity.Get<TextLayout>() };

	std::optional<TextClipConstraint> explicit_clip;

	if (auto clip{ entity.TryGet<impl::TextClip>() }; clip && clip->rect.has_value()) {
		explicit_clip = TextClipConstraint{
			.rect = clip->rect.value(),
			.mode = clip->mode == TextClipMode::None ? TextClipMode::Clip : clip->mode,
		};
	}

	auto prepared{
		impl::PrepareTextDraw(GetDrawTransform(entity), box, GetDrawOrigin(entity), explicit_clip)
	};

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
			.reveal_glyph_count = Text{ entity }.GetRevealGlyphCount(),
			.time				= scene.ctx().TimeSinceStartSeconds().count(),
		},
		effects
	);
}

Text::Text(Entity entity) : Entity{ entity } {}

StyledText& Text::GetStyledText() {
	return Get<StyledText>();
}

const StyledText& Text::GetStyledText() const {
	return Get<StyledText>();
}

TextBox& Text::GetTextBox() {
	return Get<TextBox>();
}

const TextBox& Text::GetTextBox() const {
	return Get<TextBox>();
}

Text& Text::Clear() {
	auto& styled_text{ Get<StyledText>() };
	auto& edit_state{ Get<impl::TextEditState>() };

	styled_text.runs.clear();
	styled_text.runs.emplace_back();

	edit_state.current_run_index = 0;

	InvalidateLayout();

	return *this;
}

Text& Text::Content(std::string_view content) {
	auto& styled_text{ Get<StyledText>() };
	auto& edit_state{ Get<impl::TextEditState>() };

	if (styled_text.runs.size() == 1 && styled_text.runs.front().text.empty()) {
		auto& run{ styled_text.runs.front() };
		run.text					 = std::string{ content };
		edit_state.current_run_index = 0;
		InvalidateLayout();
		return *this;
	}

	auto& run{ styled_text.runs.emplace_back() };
	run.text = std::string{ content };

	edit_state.current_run_index = styled_text.runs.size() - 1;

	InvalidateLayout();

	return *this;
}

Text& Text::Content(StyledText styled_text) {
	Add<StyledText>(std::move(styled_text));
	Get<impl::TextEditState>().current_run_index = 0;
	InvalidateLayout();
	return *this;
}

Text& Text::Select(std::size_t index) {
	const auto& styled_text{ Get<StyledText>() };

	PTGN_ASSERT(index < styled_text.runs.size(), "Invalid text run index");

	Get<impl::TextEditState>().current_run_index = index;

	return *this;
}

TextRun& Text::CurrentRun() {
	return const_cast<TextRun&>(std::as_const(*this).CurrentRun()); // NOSONAR
}

const TextRun& Text::CurrentRun() const {
	auto& styled_text{ Get<StyledText>() };
	const auto& edit_state{ Get<impl::TextEditState>() };

	PTGN_ASSERT(
		edit_state.current_run_index < styled_text.runs.size(), "Invalid current text run index"
	);

	return styled_text.runs[edit_state.current_run_index];
}

Text& Text::Box(Rect rect) {
	Get<TextBox>().rect = rect;
	InvalidateLayout();
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

Text& Text::Align(ptgn::HorizontalAlign horizontal, ptgn::VerticalAlign vertical) {
	auto& style{ Get<TextBox>().style };

	style.horizontal_align = horizontal;
	style.vertical_align   = vertical;

	auto& alignment_override{ TryAdd<impl::TextAlignmentOverride>() };
	alignment_override.horizontal = true;
	alignment_override.vertical	  = true;

	InvalidateLayout();

	return *this;
}

Text& Text::HorizontalAlign(ptgn::HorizontalAlign align) {
	Get<TextBox>().style.horizontal_align			 = align;
	TryAdd<impl::TextAlignmentOverride>().horizontal = true;

	InvalidateLayout();

	return *this;
}

Text& Text::VerticalAlign(ptgn::VerticalAlign align) {
	Get<TextBox>().style.vertical_align			   = align;
	TryAdd<impl::TextAlignmentOverride>().vertical = true;

	InvalidateLayout();

	return *this;
}

Text& Text::TabWidth(std::size_t spaces) {
	PTGN_ASSERT(spaces > 0, "Text tab width must be at least one space");
	Get<TextBox>().style.tab_width = spaces;
	InvalidateLayout();
	return *this;
}

void Text::ApplyFallbackAlignment(ptgn::HorizontalAlign horizontal, ptgn::VerticalAlign vertical) {
	auto alignment_override{ TryGet<impl::TextAlignmentOverride>() };
	auto& style{ Get<TextBox>().style };

	bool changed{ false };

	if ((!alignment_override || !alignment_override->horizontal) &&
		style.horizontal_align != horizontal) {
		style.horizontal_align = horizontal;
		changed				   = true;
	}

	if ((!alignment_override || !alignment_override->vertical) &&
		style.vertical_align != vertical) {
		style.vertical_align = vertical;
		changed				 = true;
	}

	if (changed) {
		InvalidateLayout();
	}
}

Text& Text::ClearAlignment() {
	Remove<impl::TextAlignmentOverride>();

	auto [horizontal, vertical]{ GetTextAlignment(GetDrawOrigin(*this)) };
	auto& style{ Get<TextBox>().style };
	style.horizontal_align = horizontal;
	style.vertical_align   = vertical;

	InvalidateLayout();
	return *this;
}

V2_float Text::GetSize() const {
	return GetLayout().size;
}

Rect Text::GetBounds() const {
	const auto& layout{ GetLayout() };
	auto bounds{ layout.GetBounds() };

	if (const auto& box{ GetTextBox() }; box.HasBox()) {
		auto origin_point{ box.rect.GetOriginPoint(GetDrawOrigin(*this)) };
		return bounds.Translated(-origin_point);
	}

	return bounds;
}

Text& Text::Wrap(WrapMode mode) {
	Get<TextBox>().style.wrap_mode = mode;
	InvalidateLayout();
	return *this;
}

Text& Text::Overflow(OverflowMode mode) {
	Get<TextBox>().style.overflow_mode = mode;
	InvalidateLayout();
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
	Get<TextBox>().style.collapse_spaces = collapse;
	InvalidateLayout();
	return *this;
}

Text& Text::JustifyLastLine(bool justify) {
	Get<TextBox>().style.justify_last_line = justify;
	InvalidateLayout();
	return *this;
}

Text& Text::AllowWordBreakInOverflow(bool allow) {
	Get<TextBox>().style.allow_word_break_in_overflow = allow;
	InvalidateLayout();
	return *this;
}

Text& Text::InsertHyphenOnSplit(bool insert) {
	Get<TextBox>().style.insert_hyphen_on_split = insert;
	InvalidateLayout();
	return *this;
}

Text& Text::PreventSingleLetterSplit(bool prevent) {
	Get<TextBox>().style.prevent_single_letter_split = prevent;
	InvalidateLayout();
	return *this;
}

Text& Text::RequireThreeLetterRemainder(bool require) {
	Get<TextBox>().style.require_three_letter_remainder = require;
	InvalidateLayout();
	return *this;
}

Text& Text::MaxLines(std::size_t max_lines) {
	auto& style{ Get<TextBox>().style };

	style.max_lines = max_lines;

	InvalidateLayout();

	return *this;
}

Text& Text::ScaleToFit(float min_scale, float max_scale) {
	PTGN_ASSERT(min_scale > 0.0f, "Minimum text scale must be positive");
	PTGN_ASSERT(max_scale > 0.0f, "Maximum text scale must be positive");
	PTGN_ASSERT(min_scale <= max_scale, "Minimum text scale cannot exceed maximum text scale");

	auto& style{ Get<TextBox>().style };

	style.overflow_mode	   = OverflowMode::ScaleToFit;
	style.shrink_scale.min = min_scale;
	style.shrink_scale.max = max_scale;

	InvalidateLayout();

	return *this;
}

Text& Text::Font(std::string_view font_key) {
	CurrentRun().font = std::string{ font_key };
	InvalidateLayout();
	return *this;
}

Text& Text::Color(ptgn::Color color) {
	CurrentRun().style.color = color;
	InvalidateLayout();
	return *this;
}

Text& Text::Size(float font_size) {
	CurrentRun().style.size = font_size;
	InvalidateLayout();
	return *this;
}

Text& Text::Kerning(float kerning) {
	CurrentRun().style.kerning = kerning;
	InvalidateLayout();
	return *this;
}

Text& Text::Tracking(float tracking) {
	CurrentRun().style.tracking = tracking;
	InvalidateLayout();
	return *this;
}

Text& Text::LineSpacing(float line_spacing) {
	CurrentRun().style.line_spacing = line_spacing;
	InvalidateLayout();
	return *this;
}

Text& Text::Style(FontStyle flags) {
	CurrentRun().style.flags = flags;
	InvalidateLayout();
	return *this;
}

Text& Text::Bold(bool enabled, float weight) {
	auto& style{ CurrentRun().style };

	style.flags				   = SetFlag(style.flags, FontStyle::Bold, enabled);
	style.fake_bold_if_missing = enabled;
	style.fake_bold_weight	   = weight;

	InvalidateLayout();

	return *this;
}

Text& Text::Italic(bool enabled) {
	auto& style{ CurrentRun().style };

	style.flags = SetFlag(style.flags, FontStyle::Italic, enabled);

	InvalidateLayout();

	return *this;
}

Text& Text::Underline(bool enabled) {
	auto& style{ CurrentRun().style };

	style.flags = SetFlag(style.flags, FontStyle::Underline, enabled);

	InvalidateLayout();

	return *this;
}

Text& Text::Strikethrough(bool enabled) {
	auto& style{ CurrentRun().style };

	style.flags = SetFlag(style.flags, FontStyle::Strikethrough, enabled);

	InvalidateLayout();

	return *this;
}

Text& Text::Outline(ptgn::Color color, float width, float softness) {
	auto& sdf{ CurrentRun().style.sdf };

	sdf.outline_color	 = color;
	sdf.outline_width	 = width;
	sdf.outline_softness = softness;

	InvalidateLayout();

	return *this;
}

Text& Text::Shadow(ptgn::Color color, V2_float offset, float softness) {
	return Shadow(color, offset, 0.0f, softness);
}

Text& Text::Shadow(ptgn::Color color, V2_float offset, float width, float softness) {
	auto& sdf{ CurrentRun().style.sdf };

	sdf.shadow_color	= color;
	sdf.shadow_offset	= offset;
	sdf.shadow_width	= width;
	sdf.shadow_softness = softness;

	InvalidateLayout();

	return *this;
}

Text& Text::OuterGlow(ptgn::Color color, float width, float softness) {
	auto& sdf{ CurrentRun().style.sdf };

	sdf.outer_glow_color	= color;
	sdf.outer_glow_width	= width;
	sdf.outer_glow_softness = softness;

	InvalidateLayout();

	return *this;
}

Text& Text::InnerGlow(ptgn::Color color, float width, float softness) {
	auto& sdf{ CurrentRun().style.sdf };

	sdf.inner_glow_color	= color;
	sdf.inner_glow_width	= width;
	sdf.inner_glow_softness = softness;

	InvalidateLayout();

	return *this;
}

Text& Text::ClearSdfEffects() {
	auto& sdf{ CurrentRun().style.sdf };

	auto weight{ sdf.weight };
	auto softness{ sdf.softness };
	auto pixel_range{ sdf.pixel_range };

	sdf = {};

	sdf.weight		= weight;
	sdf.softness	= softness;
	sdf.pixel_range = pixel_range;

	InvalidateLayout();

	return *this;
}

Text& Text::Effect(
	GlyphEffectType type, float amplitude, float frequency, float speed, float phase
) {
	auto& effect{ CurrentRun().style.effect };

	effect.type		 = type;
	effect.amplitude = amplitude;
	effect.frequency = frequency;
	effect.speed	 = speed;
	effect.phase	 = phase;

	InvalidateLayout();

	return *this;
}

const TextLayout& Text::GetLayout() const {
	const auto& styled_text{ GetStyledText() };
	const auto& box{ GetTextBox() };

	UpdateLayout(*this, GetScene().ctx().asset, styled_text, box);

	return Get<TextLayout>();
}

std::size_t Text::GetRevealGlyphCount() const {
	if (auto reveal{ TryGet<impl::TextReveal>() }) {
		return reveal->glyph_count;
	}

	return std::numeric_limits<std::size_t>::max();
}

void Text::InvalidateLayout() {
	if (auto layout{ TryGet<TextLayout>() }) {
		layout->hash = 0;
	}
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

std::size_t Text::GetGlyphCount() const {
	return GetLayout().GetGlyphCount();
}

std::size_t Text::GetVisibleGlyphCount() const {
	return GetLayout().GetVisibleGlyphCount();
}

bool Text::IsFullyRevealed() const {
	auto glyph_count{ GetLayout().GetVisibleGlyphCount() };

	if (auto reveal{ TryGet<impl::TextReveal>() }) {
		return reveal->glyph_count >= glyph_count;
	}

	return true;
}

Text& Text::RevealFraction(float fraction) {
	fraction = std::clamp(fraction, 0.0f, 1.0f);

	auto glyph_count{ GetVisibleGlyphCount() };

	auto reveal_count{
		static_cast<std::size_t>(std::round(static_cast<float>(glyph_count) * fraction))
	};

	return Reveal(reveal_count);
}

Text CreateText(Scene& scene, Transform transform, Origin draw_origin) {
	Text text{ scene.CreateEntity() };

	StyledText styled_text;
	styled_text.runs.emplace_back();

	auto [horizontal, vertical] = GetTextAlignment(draw_origin);
	TextBox box;
	box.style.horizontal_align = horizontal;
	box.style.vertical_align   = vertical;

	text.Add<StyledText>(std::move(styled_text));
	text.Add<TextBox>(box);
	text.Add<impl::TextEditState>(impl::TextEditState{ .current_run_index = 0 });

	SetTransform(text, transform);
	SetDrawOrigin(text, draw_origin);
	SetDraw<Text>(text);
	Show(text, true);

	return text;
}

} // namespace ptgn