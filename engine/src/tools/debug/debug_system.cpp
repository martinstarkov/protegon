#include "tools/debug/debug_system.h"

#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/text.h"
#include "tools/debug/profiling.h"
#include "tools/debug/stats.h"

namespace ptgn {

DebugContext::DebugContext(RenderContext& render_context) : render_context_{ render_context } {}

void DebugContext::DrawText(
	std::string_view content, Transform transform, Color text_color, std::optional<float> font_size,
	const std::variant<std::monostate, Font, std::string_view>& font,
	const TextProperties& properties, Origin origin, std::optional<V2_float> text_size,
	bool hd_text, std::optional<Camera> camera
) {
	// TODO: Fix.

	// auto& debug_commands{ render_context_.GetDebugCommandsForCamera(camera) };
	// debug_commands.emplace_back(...);
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

	std::visit(
		[&](const auto& cmd) { RenderContext::AddDrawCommand(debug_commands, cmd, 0.0f); },
		shape_draw_commands
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

	RenderContext::AddDrawCommand(debug_commands, line_draw_commands, 0.0f);
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