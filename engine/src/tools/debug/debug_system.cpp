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

// TODO: Fix.
// void DebugContext::DrawText(
//	std::string_view text_content, Transform transform, Color text_color, FontSize font_size,
//	FontOrKey font, const TextProperties& properties, Origin draw_origin,
//	std::optional<V2_float> text_size, const std::optional<SceneCamera>& camera
//) {
// }

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
	// TODO: Fix.
}

void DebugContext::DrawLines(
	const std::vector<V2_float>& points, Color color, float line_width, bool connect_last_to_first,
	std::optional<Transform> transform, const std::optional<SceneCamera>& camera
) {
	// TODO: Fix.
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