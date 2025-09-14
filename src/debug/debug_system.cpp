#include "debug/debug_system.h"

#include <cstdint>
#include <limits>
#include <string>

#include "components/draw.h"
#include "components/generic.h"
#include "components/transform.h"
#include "core/game.h"
#include "debug/stats.h"
#include "math/geometry/line.h"
#include "math/geometry/rect.h"
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
	const std::string& content, const Transform& og_transform, const TextColor& color,
	Origin origin, const FontSize& font_size, const ResourceHandle& font_key,
	const TextProperties& properties, const V2_float& text_size, const Camera& camera
) {
	V2_float size{ text_size };
	Transform transform{ og_transform };

	auto texture{ game.renderer.CreateTexture(
		transform, size, content, color, font_size, font_key, properties, true, camera
	) };

	Rect rect{ !size.IsZero() ? size : V2_float{ texture.GetSize() } };

	DrawTextureCommand cmd;

	cmd.transform			= transform;
	cmd.texture_id			= texture.GetId();
	cmd.texture_size		= texture.GetSize();
	cmd.texture_format		= texture.GetFormat();
	cmd.rect				= rect;
	cmd.origin				= origin;
	cmd.depth				= max_depth;
	cmd.render_state.camera = camera;

	game.renderer.render_data_.Submit(cmd, true);

	game.renderer.render_data_.AddTemporaryTexture(std::move(texture));
}

void DebugSystem::DrawShape(
	const Transform& transform, const Shape& shape, const Tint& color, const LineWidth& line_width,
	Origin origin, const Camera& camera
) {
	DrawShapeCommand cmd;

	cmd.transform				= transform;
	cmd.shape					= shape;
	cmd.tint					= color;
	cmd.line_width				= line_width;
	cmd.origin					= origin;
	cmd.depth					= max_depth;
	cmd.render_state.blend_mode = default_blend_mode;
	cmd.render_state.camera		= camera;

	game.renderer.render_data_.Submit(cmd, true);
}

void DebugSystem::DrawLine(
	const V2_float& start, const V2_float& end, const Tint& color, const LineWidth& line_width,
	const Camera& camera
) {
	DrawShape({}, Line{ start, end }, color, line_width, Origin::Center, camera);
}

void DebugSystem::DrawPoint(const V2_float& point, const Tint& color, const Camera& camera) {
	DrawShape({}, point, color, -1.0f, Origin::Center, camera);
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