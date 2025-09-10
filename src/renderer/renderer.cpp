#include "renderer/renderer.h"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/effects.h"
#include "components/generic.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/window.h"
#include "debug/log.h"
#include "math/geometry.h"
#include "math/geometry/arc.h"
#include "math/geometry/capsule.h"
#include "math/geometry/circle.h"
#include "math/geometry/ellipse.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/rounded_rect.h"
#include "math/geometry/triangle.h"
#include "math/vector2.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/buffers/frame_buffer.h"
#include "renderer/font.h"
#include "renderer/gl/gl_renderer.h"
#include "renderer/render_data.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

namespace ptgn {

namespace impl {

const Depth max_depth{ std::numeric_limits<std::int32_t>::max() };
const BlendMode debug_blend_mode{ BlendMode::Blend };

RenderState GetDebugRenderState(const Camera& camera) {
	impl::RenderState state;
	state.blend_mode = debug_blend_mode;
	state.camera	 = camera;
	state.post_fx	 = {};
	return state;
}

} // namespace impl

void DrawDebugTexture(
	const TextureHandle& texture_key, const V2_float& position, const V2_float& size, Origin origin,
	float rotation, const Camera& camera
) {
	Rect rect{ size.IsZero() ? V2_float{ texture_key.GetSize() } : size };

	impl::DrawTextureCommand cmd;

	cmd.transform	 = Transform{ position, rotation };
	cmd.texture		 = &texture_key.GetTexture();
	cmd.rect		 = rect;
	cmd.origin		 = origin;
	cmd.depth		 = impl::max_depth;
	cmd.render_state = impl::GetDebugRenderState(camera);

	game.renderer.GetRenderData().Submit(cmd);
}

void DrawDebugText(
	const std::string& content, const V2_float& position, const TextColor& color, Origin origin,
	const FontSize& font_size, bool hd_text, const ResourceHandle& font_key,
	const TextProperties& properties, const V2_float& size, float rotation, const Camera& camera
) {
	auto& render_data{ game.renderer.GetRenderData() };
	FontSize final_font_size{ font_size };
	Transform transform{ position, rotation };
	if (hd_text) {
		const auto& scene{ game.scene.GetCurrent() };
		auto render_target_scale{ scene.GetRenderTargetScaleRelativeTo(camera) };
		PTGN_ASSERT(render_target_scale.BothAboveZero());
		transform.Scale(1.0f / render_target_scale);
		final_font_size =
			static_cast<std::int32_t>(static_cast<float>(font_size) * render_target_scale.y);
	}
	auto texture{ Text::CreateTexture(content, color, final_font_size, font_key, properties) };

	Rect rect{ size.IsZero() ? V2_float{ Text::GetSize(content, font_key, final_font_size) }
							 : size };

	impl::DrawTextureCommand cmd;

	cmd.transform	 = transform;
	cmd.texture		 = &texture;
	cmd.rect		 = rect;
	cmd.origin		 = origin;
	cmd.depth		 = impl::max_depth;
	cmd.render_state = impl::GetDebugRenderState(camera);

	render_data.Submit(cmd);
	render_data.AddTemporaryTexture(std::move(texture));
}

void DrawDebugLines(
	const std::vector<V2_float>& points, const Color& color, float line_width,
	bool connect_last_to_first, const Camera& camera
) {
	impl::DrawLinesCommand cmd;

	cmd.points				  = points;
	cmd.connect_last_to_first = connect_last_to_first;
	cmd.tint				  = color;
	cmd.depth				  = impl::max_depth;
	cmd.line_width			  = line_width;
	cmd.render_state		  = impl::GetDebugRenderState(camera);

	game.renderer.GetRenderData().Submit(cmd);
}

void DrawDebugLine(
	const V2_float& line_start, const V2_float& line_end, const Color& color, float line_width,
	const Camera& camera
) {
	DrawDebugShape({}, Line{ line_start, line_end }, color, line_width, camera);
}

void DrawDebugTriangle(
	const std::array<V2_float, 3>& vertices, const Color& color, float line_width,
	const Camera& camera
) {
	DrawDebugShape({}, Triangle{ vertices }, color, line_width, camera);
}

void DrawDebugRect(
	const V2_float& position, const V2_float& size, const Color& color, Origin origin,
	float line_width, float rotation, const Camera& camera
) {
	DrawDebugShape(Transform{ position, rotation }, Rect{ size }, color, line_width, camera);
}

void DrawDebugRoundedRect(
	const V2_float& position, const V2_float& size, float radius, const Color& color, Origin origin,
	float line_width, float rotation, const Camera& camera
) {
	DrawDebugShape(
		Transform{ position, rotation }, RoundedRect{ size, radius }, color, line_width, camera
	);
}

void DrawDebugEllipse(
	const V2_float& center, const V2_float& radii, const Color& color, float line_width,
	float rotation, const Camera& camera
) {
	DrawDebugShape(Transform{ center, rotation }, Ellipse{ radii }, color, line_width, camera);
}

void DrawDebugCircle(
	const V2_float& center, float radius, const Color& color, float line_width, const Camera& camera
) {
	DrawDebugShape(Transform{ center }, Circle{ radius }, color, line_width, camera);
}

void DrawDebugCapsule(
	const V2_float& start, const V2_float& end, float radius, const Color& color, float line_width,
	const Camera& camera
) {
	DrawDebugShape({}, Capsule{ start, end, radius }, color, line_width, camera);
}

void DrawDebugArc(
	const V2_float& center, float radius, float start_angle, float end_angle, const Color& color,
	float line_width, bool clockwise, const Camera& camera
) {
	DrawDebugShape(
		Transform{ center }, Arc{ radius, start_angle, end_angle, clockwise }, color, line_width,
		camera
	);
}

void DrawDebugPolygon(
	const std::vector<V2_float>& vertices, const Color& color, float line_width,
	const Camera& camera
) {
	DrawDebugShape({}, Polygon{ vertices }, color, line_width, camera);
}

void DrawDebugPoint(const V2_float position, const Color& color, const Camera& camera) {
	DrawDebugShape({}, position, color, -1.0f, camera);
}

void DrawDebugShape(
	const Transform& transform, const Shape& shape, const Color& color, float line_width,
	const Camera& camera
) {
	impl::DrawShapeCommand cmd;

	cmd.transform	 = transform;
	cmd.shape		 = shape;
	cmd.tint		 = color;
	cmd.depth		 = impl::max_depth;
	cmd.line_width	 = line_width;
	cmd.render_state = impl::GetDebugRenderState(camera);

	game.renderer.GetRenderData().Submit(cmd);
}

namespace impl {

void Renderer::Init() {
	render_data_.Init();
}

void Renderer::SetBackgroundColor(const Color& background_color) {
	render_data_.screen_target_.SetClearColor(background_color);
}

Color Renderer::GetBackgroundColor() const {
	return render_data_.screen_target_.GetClearColor();
}

void Renderer::Reset() {
	bound_ = {};

	FrameBuffer::Unbind(); // Will set bound_frame_buffer_id_ to 0.

	render_data_ = {};
	render_data_.Init();
}

void Renderer::Shutdown() {
	Reset();
}

void Renderer::SetScalingMode(ScalingMode scaling_mode) {
	V2_int resolution{ render_data_.game_size_set_ ? render_data_.game_size_
												   : game.window.GetSize() };
	render_data_.UpdateResolutions(resolution, scaling_mode);
}

void Renderer::SetGameSize(const V2_int& game_size, ScalingMode scaling_mode) {
	render_data_.game_size_set_ = !game_size.IsZero();
	V2_int resolution{ render_data_.game_size_set_ ? game_size : game.window.GetSize() };
	render_data_.UpdateResolutions(resolution, scaling_mode);
}

V2_int Renderer::GetDisplaySize() const {
	return render_data_.display_viewport_.size;
}

V2_float Renderer::GetScale() const {
	V2_int display_size{ GetDisplaySize() };
	V2_int game_size{ GetGameSize() };
	PTGN_ASSERT(display_size.BothAboveZero());
	PTGN_ASSERT(game_size.BothAboveZero());
	return V2_float{ display_size } / game_size;
}

V2_int Renderer::GetGameSize() const {
	return render_data_.game_size_;
}

ScalingMode Renderer::GetScalingMode() const {
	return render_data_.resolution_mode_;
}

RenderData& Renderer::GetRenderData() {
	return render_data_;
}

void Renderer::PresentScreen() {
	FrameBuffer::Unbind();

	// PTGN_ASSERT(
	// 	std::invoke([]() {
	// 		auto viewport_size{ GLRenderer::GetViewportSize() };
	// 		if (viewport_size.IsZero()) {
	// 			return false;
	// 		}
	// 		if (viewport_size.x == 1 && viewport_size.y == 1) {
	// 			return false;
	// 		}
	// 		return true;
	// 	}),
	// 	"Attempting to render to 0 or 1 sized viewport"
	// );

	PTGN_ASSERT(
		FrameBuffer::IsUnbound(),
		"Frame buffer must be unbound (id=0) before swapping SDL2 buffer to the screen"
	);

	game.window.SwapBuffers();
}

void Renderer::ClearScreen() const {
	FrameBuffer::Unbind();
	GLRenderer::SetClearColor(color::Transparent);
	GLRenderer::Clear();
	render_data_.ClearScreenTarget();
}

} // namespace impl

} // namespace ptgn