#include "debug/debug_system.h"

#include <cstdint>
#include <limits>
#include <string>

#include "core/app/application.h"
#include "core/asset/asset_handle.h"
#include "core/asset/asset_manager.h"
#include "core/ecs/components/draw.h"
#include "core/ecs/components/generic.h"
#include "core/ecs/components/transform.h"
#include "debug/profiling.h"
#include "debug/stats.h"
#include "math/geometry/line.h"
#include "math/geometry/rect.h"
#include "math/vector2.h"
#include "renderer/api/origin.h"
#include "renderer/renderer.h"
#include "renderer/text/font.h"
#include "renderer/text/text.h"
#include "world/scene/camera.h"

namespace ptgn::impl {

const Depth max_depth{ std::numeric_limits<std::int32_t>::max() };

// TODO: Fix.
/*
void DebugSystem::DrawText(
	const std::string& content, const Transform& og_transform, const TextColor& color,
	Origin origin, const FontSize& font_size, const Handle<Font>& font_key,
	const TextProperties& properties, const V2_float& text_size, const Camera& camera
) {
	V2_float size{ text_size };
	Transform transform{ og_transform };

	auto texture{ CreateTexture(
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

	Application::Get().render_.render_data_.Submit(cmd, true);

	Application::Get().render_.render_data_.AddTemporaryTexture(std::move(texture));
}*/

DebugSystem::DebugSystem(Renderer& renderer) : renderer_{ renderer } {}

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

	renderer_.Submit(cmd, true);
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

void DebugSystem::PreUpdate() {
	GetProfiler().timings_.clear();
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