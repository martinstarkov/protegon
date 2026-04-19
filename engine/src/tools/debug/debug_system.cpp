#include "tools/debug/debug_system.h"

#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/vertex/vertex.h"
#include "runtime/asset/asset.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/text.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "tools/debug/profiling.h"
#include "tools/debug/stats.h"

namespace ptgn {

DebugContext::DebugContext(RenderContext& render_context) : render_context_{ render_context } {}

void DebugContext::DrawText(
	std::string_view text_content, Transform transform, Color text_color, FontSize font_size,
	FontOrKey font, const TextProperties& properties, Origin draw_origin,
	std::optional<V2_float> text_size, const std::optional<SceneCamera>& camera
) {
	auto texture_object{ render_context_.scene_.ctx().asset.CreateTextTextureObject(
		text_content, text_color, font_size, font, properties
	) };

	if (!texture_object.has_value()) {
		return;
	}

	auto texture_size{ texture_object->GetSize() };

	auto texture_id{ texture_object->operator impl::TextureId() };

	render_context_.temporary_textures_.emplace_back(std::move(*texture_object));

	auto quad_shader{ render_context_.renderer_.GetShader("quad") };

	auto& debug_commands{ render_context_.GetDebugCommandsForCamera(
		camera.transform([](const auto& c) { return impl::RenderCamera{ c }; })
	) };

	Rect rect{ text_size.value_or(texture_size) };

	auto positions{ rect.GetWorldVertices(transform, draw_origin) };

	auto tex_coords{ impl::GetDefaultTextureCoordinates<false>() };

	constexpr bool floor_positions{ true };

	impl::TextureCommand texture_command{ quad_shader,	  texture_id, positions,
										  color::White,	  tex_coords, debug_blend_mode,
										  floor_positions };

	debug_commands.emplace_back(texture_command, debug_depth);
}

void DebugContext::DrawShape(
	const Shape& shape, Transform transform, Color color, FillStyle fill_style, Origin draw_origin,
	const std::optional<SceneCamera>& camera
) {
	return DrawShape(
		shape, transform, color, fill_style, draw_origin,
		camera.transform([](auto& c) { return impl::RenderCamera{ c }; })
	);
}

void DebugContext::DrawShape(
	const Shape& shape, Transform transform, Color color, FillStyle fill_style, Origin draw_origin,
	const std::optional<impl::RenderCamera>& camera
) {
	auto shape_draw_commands{ DrawContext::GetDrawCommand(
		render_context_.renderer_, shape, transform, color, fill_style, draw_origin,
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
	std::optional<Transform> transform, const std::optional<SceneCamera>& camera
) {
	auto& debug_commands{ render_context_.GetDebugCommandsForCamera(
		camera.transform([](const auto& c) { return impl::RenderCamera{ c }; })
	) };

	constexpr bool floor_positions{ false };

	auto draw_commands{ DrawContext::GetDrawCommand(
		points, line_width, transform.value_or(Transform{}), color, debug_blend_mode,
		connect_last_to_first, floor_positions
	) };

	if (!draw_commands.has_value()) {
		return;
	}

	PTGN_ASSERT(std::holds_alternative<std::vector<impl::QuadCommand>>(*draw_commands));

	const auto& line_draw_commands{ std::get<std::vector<impl::QuadCommand>>(*draw_commands) };

	RenderContext::AddDrawCommand(debug_commands, line_draw_commands, debug_depth);
}

void DebugContext::DrawLine(
	V2_float start, V2_float end, Color color, float line_width,
	const std::optional<SceneCamera>& camera
) {
	PTGN_ASSERT(line_width >= kMinLineWidth, "Line width must be at least ", kMinLineWidth);
	DrawShape(Line{ start, end }, {}, color, line_width, Origin::Center, camera);
}

void DebugContext::DrawPoint(
	V2_float point, Color color, const std::optional<SceneCamera>& camera
) {
	DrawShape(point, {}, color, 1.0f, Origin::Center, camera);
}

void DebugContext::DrawPoint(
	V2_float point, Color color, const std::optional<impl::RenderCamera>& camera
) {
	DrawShape(point, {}, color, 1.0f, Origin::Center, camera);
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