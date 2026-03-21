#include "tools/debug/debug_system.h"

#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "app/context.h"
#include "core/assert.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/texture.h"
#include "renderer/primitives/vertex.h"
#include "renderer/renderer.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/font_system.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/text.h"
#include "runtime/scene/scene.h"
#include "tools/debug/profiling.h"
#include "tools/debug/stats.h"

namespace ptgn {

DebugContext::DebugContext(RenderContext& render_context) : render_context_{ render_context } {}

void DebugContext::DrawText(
	std::string_view text_content, Transform transform, Color text_color,
	std::optional<float> font_size, const std::optional<std::variant<Font, std::string_view>>& font,
	const TextProperties& properties, Origin draw_origin, std::optional<V2_float> text_size,
	bool hd_text, std::optional<Camera> camera
) {
	PTGN_ASSERT(
		render_context_.scene_ != nullptr && render_context_.renderer_ != nullptr,
		"Render context must be initialized before use"
	);

	// TODO: Most of this code is duplicated with RenderContext::DrawText and Text::Draw. Consider
	// moving the common parts to a helper function.

	auto resolved_font{ render_context_.scene_->app().asset.ToFont(font) };

	float hd_scale{ hd_text ? impl::GetTextScale(*render_context_.scene_, camera) : 1.0f };

	float resolved_font_size{ font_size.value_or(kDefaultFontSize) };

	if (hd_text) {
		resolved_font_size *= hd_scale;

		auto scale{ impl::GetCameraParentRenderTargetScale(*render_context_.scene_, camera) };

		PTGN_ASSERT(!scale.HasZero(), "Scale cannot have a zero component");

		transform.Scale(transform.GetScale() / scale);
	}

	auto texture_object{ render_context_.scene_->app().asset.CreateTextTextureObject(
		text_content, text_color, resolved_font_size, resolved_font.value_or(Font{}), properties,
		hd_scale, hd_text
	) };

	if (!texture_object.has_value()) {
		return;
	}

	auto texture_size{ texture_object->GetSize() };

	auto texture_id{ texture_object->operator impl::TextureId() };

	render_context_.temporary_textures_.emplace_back(std::move(*texture_object));

	auto quad_shader{ render_context_.renderer_->GetShader("quad") };

	auto& debug_commands{ render_context_.GetDebugCommandsForCamera(camera) };

	Rect rect{ text_size.value_or(texture_size) };

	auto positions{ rect.GetWorldVertices(transform, draw_origin) };

	auto tex_coords{ impl::GetDefaultTextureCoordinates<false>() };

	impl::TextureCommand texture_command{ quad_shader,	texture_id, positions,
										  color::White, tex_coords, debug_blend_mode };

	debug_commands.emplace_back(texture_command, debug_depth);
}

void DebugContext::DrawShape(
	const Shape& shape, Transform transform, Color color, FillStyle fill_style, Origin draw_origin,
	std::optional<Camera> camera
) {
	PTGN_ASSERT(
		render_context_.renderer_ != nullptr, "Render context must be initialized before use"
	);

	auto shape_draw_commands{ DrawContext::GetShapeDrawCommand(
		*render_context_.renderer_, shape, transform, color, fill_style, draw_origin,
		debug_blend_mode
	) };

	auto& debug_commands{ render_context_.GetDebugCommandsForCamera(camera) };

	if (!shape_draw_commands.has_value()) {
		return;
	}

	std::visit(
		[&](const auto& cmd) { RenderContext::AddDrawCommand(debug_commands, cmd, debug_depth); },
		*shape_draw_commands
	);
}

void DebugContext::DrawLines(
	const std::vector<V2_float>& points, Color color, float line_width, bool connect_last_to_first,
	std::optional<Transform> transform, std::optional<Camera> camera
) {
	auto& debug_commands{ render_context_.GetDebugCommandsForCamera(camera) };

	auto line_draw_commands{ DrawContext::GetLineDrawCommands(
		points, line_width, transform.value_or(Transform{}), color, debug_blend_mode,
		connect_last_to_first
	) };

	RenderContext::AddDrawCommand(debug_commands, line_draw_commands, debug_depth);
}

void DebugContext::DrawLine(
	V2_float start, V2_float end, Color color, float line_width, std::optional<Camera> camera
) {
	DrawShape(Line{ start, end }, {}, color, FillStyle::Hollow(line_width), Origin::Center, camera);
}

void DebugContext::DrawPoint(V2_float point, Color color, std::optional<Camera> camera) {
	DrawShape(point, {}, color, FillStyle::Hollow(1.0f), Origin::Center, camera);
}

DebugSystem::DebugSystem() {}

void DebugSystem::PreUpdate() {
	impl::GetProfiler().timings_.clear();
}

void DebugSystem::PostUpdate() {
	// stats.PrintCollisionOverlap();
	// stats.PrintCollisionIntersect();
	// stats.PrintCollisionRaycast();
	// stats.PrintRenderer();
	// PTGN_LOG("--------------------------------------");
	// profiler.PrintAll();

	stats.Reset();
}

} // namespace ptgn