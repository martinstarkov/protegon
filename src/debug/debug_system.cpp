#include "debug/debug_system.h"

#include <cstdint>
#include <limits>
#include <string>

#include "components/draw.h"
#include "components/generic.h"
#include "components/transform.h"
#include "core/game.h"
#include "debug/stats.h"
#include "math/vector2.h"
#include "renderer/api/origin.h"
#include "renderer/font.h"
#include "renderer/render_data.h"
#include "renderer/renderer.h"
#include "renderer/text.h"
#include "resources/resource_manager.h"
#include "scene/camera.h"

namespace ptgn::impl {

const Depth max_depth{ std::numeric_limits<std::int32_t>::max() };

void DebugSystem::DrawText(
	const std::string& content, const Transform& transform, const TextColor& color, Origin origin,
	const FontSize& font_size, const ResourceHandle& font_key, const TextProperties& properties,
	const V2_float& text_size, const Camera& camera
) {
	game.renderer.DrawText(
		content, transform, color, origin, font_size, font_key, properties, text_size, {}, true,
		max_depth, default_blend_mode, camera
	);
}

void DebugSystem::DrawShape(
	const Transform& transform, const Shape& shape, const Tint& color, const LineWidth& line_width,
	Origin origin, const Camera& camera
) {
	game.renderer.DrawShape(
		transform, shape, color, line_width, origin, max_depth, default_blend_mode, camera
	);
}

void DebugSystem::DrawLine(
	const V2_float& start, const V2_float& end, const Tint& color, const LineWidth& line_width,
	const Camera& camera
) {
	game.renderer.DrawLine(start, end, color, line_width, max_depth, default_blend_mode, camera);
}

void DebugSystem::DrawPoint(const V2_float& point, const Tint& color, const Camera& camera) {
	game.renderer.DrawPoint(point, color, max_depth, default_blend_mode, camera);
}

void DebugSystem::Shutdown() {
	profiler.Reset();
}

void DebugSystem::PreUpdate() {
	profiler.Clear();
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

} // namespace ptgn::impl