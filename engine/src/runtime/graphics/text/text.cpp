#include "runtime/graphics/text/text.h"

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "renderer/draw_context.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_queue.h"
#include "runtime/graphics/text/font_style.h"
#include "runtime/graphics/text/text_effect.h"
#include "runtime/graphics/text/text_layout.h"
#include "runtime/graphics/text/text_style.h"
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

TextBox MakeDefaultTextBox() {
	return TextBox{ .rect{ { 0.0f, 0.0f }, { 0.0f, 0.0f } }, .style{} };
}

TextRunStyle MakeDefaultTextRunStyle() {
	TextRunStyle style;

	style.font	= {};
	style.color = color::White;
	style.scale = 48.0f;

	return style;
}

[[nodiscard]] bool HasVisibleTextContent(const StyledText& styled_text) {
	return std::ranges::any_of(styled_text.runs, [](const TextRun& run) {
		return !run.text.empty();
	});
}

[[nodiscard]] Transform GetTextLayoutBoxTransform(Entity entity, const Rect& local_box) {
	auto transform{ GetDrawTransform(entity) };

	V2_float center{ (local_box.min + local_box.max) * 0.5f };
	transform.Translate(center);

	return transform;
}

[[nodiscard]] Rect GetCenteredRect(V2_float size) {
	return Rect{ { -size.x * 0.5f, -size.y * 0.5f }, { size.x * 0.5f, size.y * 0.5f } };
}

void DrawTextLayoutDebugForCamera(
	Scene& scene, const impl::RenderCamera& camera, const impl::EntityFilterFunc& filter,
	TextDebugSettings settings
) {
	PTGN_ASSERT(settings.draw_enabled);

	for (auto [entity, styled_text, box] : scene.EntitiesWith<StyledText, TextBox>()) {
		if (filter(entity)) {
			continue;
		}

		if (!IsVisible(entity)) {
			continue;
		}

		if (!HasVisibleTextContent(styled_text)) {
			continue;
		}

		impl::UpdateLayout(entity, scene.ctx().asset, styled_text, box);

		const auto* layout{ entity.TryGet<TextLayout>() };
		if (!layout) {
			continue;
		}

		if (!layout->local_box.GetSize().IsPositive()) {
			continue;
		}

		auto origin_point{ impl::GetTextOriginPoint(layout->local_box, GetDrawOrigin(entity)) };
		auto box_center{ (layout->local_box.min + layout->local_box.max) * 0.5f };

		auto transform{ GetDrawTransform(entity) };
		transform.Translate(box_center - origin_point);

		auto rect{ Rect{
			{ -layout->local_box.GetSize().x * 0.5f, -layout->local_box.GetSize().y * 0.5f },
			{ layout->local_box.GetSize().x * 0.5f, layout->local_box.GetSize().y * 0.5f },
		} };

		scene.ctx().render_queue.DrawShape(
			transform, rect, settings.draw_color,
			ShapeRenderParams{
				.fill_style = settings.draw_line_width,
				.origin		= Origin::Center,
				.camera		= camera,
				.debug		= true,
			}
		);
	}
}

} // namespace

namespace impl {

void DrawTextLayoutDebug(Scene& scene) {
	TextDebugSettings settings{ scene.ctx().debug.text };

	if (scene.ctx().debug_local.text.draw_enabled) {
		settings = scene.ctx().debug_local.text;
	}

	if (!settings.draw_enabled) {
		return;
	}

	const auto& primary_world_camera{ scene.ctx().renderer.GetPrimaryWorldCamera() };

	ForDrawableSceneEntities(
		scene, primary_world_camera,
		[&settings](Scene& scene, const RenderCamera& camera, const EntityFilterFunc& filter) {
			DrawTextLayoutDebugForCamera(scene, camera, filter, settings);
		}
	);
}

} // namespace impl

void Text::Draw(
	DrawContext& ctx, Entity entity, V2_int text_size, ptgn::Color additional_tint,
	Origin offset_origin, V2_float offset_size
) {
	auto& scene{ entity.GetScene() };
	auto& assets{ scene.ctx().asset };

	impl::DrawText(assets, ctx, entity);
	// TODO: Move out.
	// static TextSystem text_system;
	// static impl::MsdfFontData msdf_font{
	//	renderer.renderer_, "assets/fonts/LiberationSans-Regular.ttf", 0, {}
	//};
	/*draw_context.DrawTexture(
		msdf_font.GetAtlasTexture(), {}, 0.0f, msdf_font.GetAtlasSize(), Origin::Center,
		color::White, impl::GetDefaultTextureCoordinates<false>(), std::nullopt, -1
	);*/

	// text_system.DrawText(renderer, {}, &msdf_font);

	/*
	Text text{ entity };

	if (!text.Has<impl::TextContent>()) {
		return;
	}

	if (text.Get<impl::TextContent>().GetValue().empty()) {
		return;
	}

	if (text.Has<impl::TextColor>() && text.Get<impl::TextColor>().a == 0) {
		return;
	}

	impl::Tint tint{ GetTint(text) };
	Transform transform{ GetDrawTransform(text) };

	if (tint.a == 0 || additional_tint.a == 0) {
		return;
	}

	// Offset text so it is centered on the offset origin and size.
	const auto transform_scale{ transform.GetScale() };
	auto scaled_offset{ offset_size * Abs(transform_scale) };
	V2_float offset{ GetOffset(offset_origin, scaled_offset) };
	transform.Translate(offset);

	const auto& text_texture{ text.Get<Texture>() };

	if (!text_texture) {
		return;
	}

	V2_int size{ text_size };

	// If the text texture size for any text_size dimension that is zero.
	if (size.HasZero()) {
		V2_int texture_size{ text_texture.GetSize() };
		if (!size.x) {
			size.x = texture_size.x;
		}
		if (!size.y) {
			size.y = texture_size.y;
		}
	}

	auto tex_coords{ GetTextureCoordinates(text, false) };

	Color text_tint{ additional_tint.Normalized() * tint.Normalized() };
	auto blend_mode{ GetBlendMode(text) };
	auto draw_origin{ GetDrawOrigin(text) };
	auto depth{ GetDepth(text) };

	// NOSONAR
	// Enable to see outline of text:
	// entity.GetScene().ctx().render_queue.DrawShape(
	//	transform, Rect{ size }, color::Purple, { .origin = draw_origin }
	//);

	renderer.DrawTexture(
		text_texture, transform, depth, size, draw_origin, text_tint, tex_coords, blend_mode,
		text.GetUUID()
	);
	*/
}

void Text::Draw(DrawContext& ctx, Entity text) {
	// This wrapper exists so that buttons can draw offset text.
	Draw(ctx, text, V2_float{}, color::White, Origin::Center, V2_float{});
}

Text::Text(Entity entity) : Entity{ entity } {}

TextRunStyle Text::MakeDefaultRunStyle() const {
	return MakeDefaultTextRunStyle();
}

StyledText& Text::EnsureStyledText() {
	if (!Has<StyledText>()) {
		Add<StyledText>();
	}

	auto& styled_text{ Get<StyledText>() };

	if (styled_text.runs.empty()) {
		auto& run{ styled_text.runs.emplace_back() };
		run.style = MakeDefaultRunStyle();
	}

	return styled_text;
}

const StyledText& Text::RequireStyledText() const {
	auto& styled_text{ Get<StyledText>() };

	PTGN_ASSERT(!styled_text.runs.empty(), "Text must always contain at least one run");

	return styled_text;
}

TextBox& Text::EnsureTextBox() {
	if (!Has<TextBox>()) {
		Add<TextBox>(MakeDefaultTextBox());
	}

	return Get<TextBox>();
}

const TextBox& Text::RequireTextBox() const {
	return Get<TextBox>();
}

impl::TextEditState& Text::EnsureEditState() {
	return TryAdd<impl::TextEditState>();
}

const impl::TextEditState& Text::RequireEditState() const {
	return Get<impl::TextEditState>();
}

void Text::EnsureValidRuns() {
	auto& styled_text{ EnsureStyledText() };
	auto& edit_state{ EnsureEditState() };

	if (styled_text.runs.empty()) {
		auto& run{ styled_text.runs.emplace_back() };
		run.style					 = MakeDefaultRunStyle();
		edit_state.current_run_index = 0;
		return;
	}

	if (edit_state.current_run_index >= styled_text.runs.size()) {
		edit_state.current_run_index = styled_text.runs.size() - 1;
	}
}

StyledText& Text::GetStyledText() {
	EnsureValidRuns();
	return Get<StyledText>();
}

const StyledText& Text::GetStyledText() const {
	return RequireStyledText();
}

TextBox& Text::GetTextBox() {
	return EnsureTextBox();
}

const TextBox& Text::GetTextBox() const {
	return RequireTextBox();
}

bool Text::HasOnlyDefaultEmptyRun() const {
	if (!Has<StyledText>()) {
		return false;
	}

	auto& styled_text{ Get<StyledText>() };

	return styled_text.runs.size() == 1 && styled_text.runs.front().text.empty();
}

Text& Text::Clear() {
	auto& styled_text{ EnsureStyledText() };
	auto& edit_state{ EnsureEditState() };

	styled_text.runs.clear();

	auto& run{ styled_text.runs.emplace_back() };
	run.style = MakeDefaultRunStyle();

	edit_state.current_run_index = 0;

	InvalidateLayout();

	return *this;
}

Text& Text::Content(std::string_view content) {
	auto& styled_text{ EnsureStyledText() };
	auto& edit_state{ EnsureEditState() };

	if (HasOnlyDefaultEmptyRun()) {
		auto& run{ styled_text.runs.front() };
		run.text					 = std::string{ content };
		edit_state.current_run_index = 0;
		InvalidateLayout();
		return *this;
	}

	auto style{ styled_text.runs.back().style };

	auto& run{ styled_text.runs.emplace_back() };
	run.text  = std::string{ content };
	run.style = style;

	edit_state.current_run_index = styled_text.runs.size() - 1;

	InvalidateLayout();

	return *this;
}

Text& Text::Select(std::size_t index) {
	auto& styled_text{ EnsureStyledText() };

	PTGN_ASSERT(index < styled_text.runs.size(), "Invalid text run index");

	EnsureEditState().current_run_index = index;

	return *this;
}

TextRun& Text::CurrentRun() {
	EnsureValidRuns();

	auto& styled_text{ Get<StyledText>() };
	auto& edit_state{ Get<impl::TextEditState>() };

	return styled_text.runs[edit_state.current_run_index];
}

const TextRun& Text::CurrentRun() const {
	auto& styled_text{ RequireStyledText() };
	auto& edit_state{ RequireEditState() };

	PTGN_ASSERT(
		edit_state.current_run_index < styled_text.runs.size(), "Invalid current text run index"
	);

	return styled_text.runs[edit_state.current_run_index];
}

TextRunStyle& Text::CurrentStyle() {
	return CurrentRun().style;
}

const TextRunStyle& Text::CurrentStyle() const {
	return CurrentRun().style;
}

Text& Text::Box(Rect rect) {
	EnsureTextBox().rect = rect;
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
	auto& style{ EnsureTextBox().style };

	style.horizontal_align = horizontal;
	style.vertical_align   = vertical;

	InvalidateLayout();

	return *this;
}

Text& Text::HorizontalAlign(ptgn::HorizontalAlign align) {
	EnsureTextBox().style.horizontal_align = align;
	InvalidateLayout();
	return *this;
}

Text& Text::VerticalAlign(ptgn::VerticalAlign align) {
	EnsureTextBox().style.vertical_align = align;
	InvalidateLayout();
	return *this;
}

Text& Text::Wrap(WrapMode mode) {
	EnsureTextBox().style.wrap_mode = mode;
	InvalidateLayout();
	return *this;
}

Text& Text::Overflow(OverflowMode mode) {
	EnsureTextBox().style.overflow_mode = mode;
	InvalidateLayout();
	return *this;
}

Text& Text::CollapseSpaces(bool collapse) {
	EnsureTextBox().style.collapse_spaces = collapse;
	InvalidateLayout();
	return *this;
}

Text& Text::JustifyLastLine(bool justify) {
	EnsureTextBox().style.justify_last_line = justify;
	InvalidateLayout();
	return *this;
}

Text& Text::AllowWordBreakInOverflow(bool allow) {
	EnsureTextBox().style.allow_word_break_in_overflow = allow;
	InvalidateLayout();
	return *this;
}

Text& Text::MaxLines(std::size_t max_lines) {
	auto& style{ EnsureTextBox().style };

	style.max_lines = max_lines;

	InvalidateLayout();

	return *this;
}

Text& Text::ScaleToFit(float min_scale, float max_scale) {
	auto& style{ EnsureTextBox().style };

	style.overflow_mode	   = OverflowMode::ScaleToFit;
	style.min_shrink_scale = min_scale;
	style.max_shrink_scale = max_scale;

	InvalidateLayout();

	return *this;
}

Text& Text::Font(std::string_view font_key) {
	CurrentStyle().font = std::string{ font_key };
	InvalidateLayout();
	return *this;
}

Text& Text::Color(ptgn::Color color) {
	CurrentStyle().color = color;
	InvalidateLayout();
	return *this;
}

Text& Text::Size(float size) {
	CurrentStyle().scale = size;
	InvalidateLayout();
	return *this;
}

Text& Text::Kerning(float kerning) {
	CurrentStyle().kerning = kerning;
	InvalidateLayout();
	return *this;
}

Text& Text::Tracking(float tracking) {
	CurrentStyle().tracking = tracking;
	InvalidateLayout();
	return *this;
}

Text& Text::LineSpacing(float line_spacing) {
	CurrentStyle().line_spacing = line_spacing;
	InvalidateLayout();
	return *this;
}

Text& Text::Style(FontStyle flags) {
	CurrentStyle().flags = flags;
	InvalidateLayout();
	return *this;
}

Text& Text::Bold(bool enabled, float weight) {
	auto& style{ CurrentStyle() };

	style.flags				   = SetFlag(style.flags, FontStyle::Bold, enabled);
	style.fake_bold_if_missing = enabled;
	style.fake_bold_weight	   = weight;

	InvalidateLayout();

	return *this;
}

Text& Text::Italic(bool enabled) {
	auto& style{ CurrentStyle() };

	style.flags = SetFlag(style.flags, FontStyle::Italic, enabled);

	InvalidateLayout();

	return *this;
}

Text& Text::Underline(bool enabled) {
	auto& style{ CurrentStyle() };

	style.flags = SetFlag(style.flags, FontStyle::Underline, enabled);

	InvalidateLayout();

	return *this;
}

Text& Text::Strikethrough(bool enabled) {
	auto& style{ CurrentStyle() };

	style.flags = SetFlag(style.flags, FontStyle::Strikethrough, enabled);

	InvalidateLayout();

	return *this;
}

Text& Text::Outline(ptgn::Color color, float width, float softness) {
	auto& sdf{ CurrentStyle().sdf };

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
	auto& sdf{ CurrentStyle().sdf };

	sdf.shadow_color	= color;
	sdf.shadow_offset	= offset;
	sdf.shadow_width	= width;
	sdf.shadow_softness = softness;

	InvalidateLayout();

	return *this;
}

Text& Text::OuterGlow(ptgn::Color color, float width, float softness) {
	auto& sdf{ CurrentStyle().sdf };

	sdf.outer_glow_color	= color;
	sdf.outer_glow_width	= width;
	sdf.outer_glow_softness = softness;

	InvalidateLayout();

	return *this;
}

Text& Text::InnerGlow(ptgn::Color color, float width, float softness) {
	auto& sdf{ CurrentStyle().sdf };

	sdf.inner_glow_color	= color;
	sdf.inner_glow_width	= width;
	sdf.inner_glow_softness = softness;

	InvalidateLayout();

	return *this;
}

Text& Text::Glow(ptgn::Color color, float width, float softness) {
	auto& sdf{ CurrentStyle().sdf };

	sdf.outer_glow_color	= color;
	sdf.outer_glow_width	= width;
	sdf.outer_glow_softness = softness;

	sdf.inner_glow_color	= color;
	sdf.inner_glow_width	= width;
	sdf.inner_glow_softness = softness;

	InvalidateLayout();

	return *this;
}

Text& Text::Glow(ptgn::Color color, float outer_width, float inner_width, float softness) {
	auto& sdf{ CurrentStyle().sdf };

	sdf.outer_glow_color	= color;
	sdf.outer_glow_width	= outer_width;
	sdf.outer_glow_softness = softness;

	sdf.inner_glow_color	= color;
	sdf.inner_glow_width	= inner_width;
	sdf.inner_glow_softness = softness;

	InvalidateLayout();

	return *this;
}

Text& Text::ClearSdfEffects() {
	auto& sdf{ CurrentStyle().sdf };

	sdf.outline_color	 = color::Black.WithAlpha(0);
	sdf.outline_width	 = 0.0f;
	sdf.outline_softness = 1.0f;

	sdf.shadow_color	= color::Black.WithAlpha(0);
	sdf.shadow_offset	= {};
	sdf.shadow_width	= 0.0f;
	sdf.shadow_softness = 1.0f;

	sdf.outer_glow_color	= color::White.WithAlpha(0);
	sdf.outer_glow_width	= 0.0f;
	sdf.outer_glow_softness = 1.0f;

	sdf.inner_glow_color	= color::White.WithAlpha(0);
	sdf.inner_glow_width	= 0.0f;
	sdf.inner_glow_softness = 1.0f;

	InvalidateLayout();

	return *this;
}

Text& Text::Effect(
	GlyphEffectType type, float amplitude, float frequency, float speed, float phase
) {
	auto& effect{ CurrentStyle().effect };

	effect.type		 = type;
	effect.amplitude = amplitude;
	effect.frequency = frequency;
	effect.speed	 = speed;
	effect.phase	 = phase;

	InvalidateLayout();

	return *this;
}

std::size_t Text::GetRunCount() const {
	return RequireStyledText().runs.size();
}

std::size_t Text::GetCurrentRunIndex() const {
	return RequireEditState().current_run_index;
}

void Text::InvalidateLayout() {
	if (auto layout{ TryGet<TextLayout>() }) {
		layout->hash = 0;
	}
}

Text CreateText(Scene& scene, V2_float position, Origin draw_origin) {
	Text text{ scene.CreateEntity() };

	StyledText styled_text;

	TextRun run;
	run.style = MakeDefaultTextRunStyle();

	styled_text.runs.push_back(std::move(run));

	text.Add<StyledText>(std::move(styled_text));
	text.Add<TextBox>(MakeDefaultTextBox());
	text.Add<impl::TextEditState>(impl::TextEditState{ .current_run_index = 0 });

	SetPosition(text, position);
	SetDrawOrigin(text, draw_origin);
	SetDraw<Text>(text);
	Show(text, true);

	return text;
}

} // namespace ptgn