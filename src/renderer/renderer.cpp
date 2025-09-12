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
#include "math/geometry/arc.h"
#include "math/geometry/capsule.h"
#include "math/geometry/circle.h"
#include "math/geometry/ellipse.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/rounded_rect.h"
#include "math/geometry/shape.h"
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

void Renderer::DrawTexture(
	const Texture& texture, const Transform& transform, const V2_float& texture_size, Origin origin,
	const Tint& tint, const Depth& depth, BlendMode blend_mode, const Camera& camera,
	const PreFX& pre_fx, const PostFX& post_fx, const std::array<V2_float, 4>& texture_coordinates
) {
	Rect rect{ !texture_size.IsZero() ? texture_size : V2_float{ texture.GetSize() } };

	DrawTextureCommand cmd;

	cmd.transform				= transform;
	cmd.texture_id				= texture.GetId();
	cmd.texture_size			= texture.GetSize();
	cmd.texture_format			= texture.GetFormat();
	cmd.rect					= rect;
	cmd.origin					= origin;
	cmd.depth					= depth;
	cmd.pre_fx					= pre_fx;
	cmd.tint					= tint;
	cmd.texture_coordinates		= texture_coordinates;
	cmd.render_state.blend_mode = blend_mode;
	cmd.render_state.camera		= camera;
	cmd.render_state.post_fx	= post_fx;

	render_data_.Submit(cmd);
}

void Renderer::DrawTexture(
	const TextureHandle& texture_key, const Transform& transform, const V2_float& texture_size,
	Origin origin, const Tint& tint, const Depth& depth, BlendMode blend_mode, const Camera& camera,
	const PreFX& pre_fx, const PostFX& post_fx, const std::array<V2_float, 4>& texture_coordinates
) {
	DrawTexture(
		texture_key.GetTexture(), transform, texture_size, origin, tint, depth, blend_mode, camera,
		pre_fx, post_fx, texture_coordinates
	);
}

void Renderer::DrawLines(
	const Transform& transform, const std::vector<V2_float>& line_points, const Tint& color,
	const LineWidth& line_width, bool connect_last_to_first, const Depth& depth,
	BlendMode blend_mode, const Camera& camera, const PostFX& post_fx
) {
	DrawLinesCommand cmd;

	cmd.transform				= transform;
	cmd.points					= line_points;
	cmd.tint					= color;
	cmd.line_width				= line_width;
	cmd.connect_last_to_first	= connect_last_to_first;
	cmd.depth					= depth;
	cmd.render_state.blend_mode = blend_mode;
	cmd.render_state.camera		= camera;
	cmd.render_state.post_fx	= post_fx;

	render_data_.Submit(cmd);
}

void Renderer::DrawLines(
	const std::vector<V2_float>& line_points, const Tint& color, const LineWidth& line_width,
	bool connect_last_to_first, const Depth& depth, BlendMode blend_mode, const Camera& camera,
	const PostFX& post_fx
) {
	DrawLines(
		{}, line_points, color, line_width, connect_last_to_first, depth, blend_mode, camera,
		post_fx
	);
}

void Renderer::DrawShape(
	const Transform& transform, const Shape& shape, const Tint& color, const LineWidth& line_width,
	Origin origin, const Depth& depth, BlendMode blend_mode, const Camera& camera,
	const PostFX& post_fx, const ShaderPass& shader_pass
) {
	DrawShapeCommand cmd;

	cmd.transform				 = transform;
	cmd.shape					 = shape;
	cmd.tint					 = color;
	cmd.line_width				 = line_width;
	cmd.origin					 = origin;
	cmd.depth					 = depth;
	cmd.render_state.shader_pass = shader_pass;
	cmd.render_state.blend_mode	 = blend_mode;
	cmd.render_state.camera		 = camera;
	cmd.render_state.post_fx	 = post_fx;

	render_data_.Submit(cmd);
}

void Renderer::DrawShader(
	const ShaderPass& shader_pass, const Entity& entity, bool clear_between_consecutive_calls,
	const Color& target_clear_color, const TextureOrSize& texture_or_size,
	BlendMode intermediate_blend_mode, const Depth& depth, BlendMode blend_mode,
	const Camera& camera, TextureFormat texture_format, const PostFX& post_fx
) {
	DrawShaderCommand cmd;

	cmd.entity							= entity;
	cmd.clear_between_consecutive_calls = clear_between_consecutive_calls;
	cmd.target_clear_color				= target_clear_color;
	cmd.texture_or_size					= texture_or_size;
	cmd.intermediate_blend_mode			= intermediate_blend_mode;
	cmd.depth							= depth;
	cmd.texture_format					= texture_format;
	cmd.render_state.shader_pass		= shader_pass;
	cmd.render_state.post_fx			= post_fx;
	cmd.render_state.blend_mode			= blend_mode;
	cmd.render_state.camera				= camera;

	render_data_.Submit(cmd);
}

void Renderer::DrawText(
	const std::string& content, Transform transform, const TextColor& color, Origin origin,
	const FontSize& font_size, const ResourceHandle& font_key, const TextProperties& properties,
	const V2_float& text_size, const Tint& tint, bool hd_text, const Depth& depth,
	BlendMode blend_mode, const Camera& camera, const PreFX& pre_fx, const PostFX& post_fx,
	const std::array<V2_float, 4>& texture_coordinates
) {
	FontSize final_font_size{ font_size };

	if (hd_text) {
		const auto& scene{ game.scene.GetCurrent() };

		auto render_target_scale{ scene.GetRenderTargetScaleRelativeTo(camera) };

		PTGN_ASSERT(render_target_scale.BothAboveZero());

		transform.Scale(1.0f / render_target_scale);

		final_font_size =
			static_cast<std::int32_t>(static_cast<float>(font_size) * render_target_scale.y);
	}

	auto texture{ Text::CreateTexture(content, color, final_font_size, font_key, properties) };

	auto texture_size{ !text_size.IsZero()
						   ? text_size
						   : V2_float{ Text::GetSize(content, font_key, final_font_size) } };

	DrawTexture(
		texture, transform, texture_size, origin, tint, depth, blend_mode, camera, pre_fx, post_fx,
		texture_coordinates
	);

	render_data_.AddTemporaryTexture(std::move(texture));
}

void Renderer::DrawRect(
	const Transform& transform, const Rect& rect, const Tint& color, const LineWidth& line_width,
	Origin origin, const Depth& depth, BlendMode blend_mode, const Camera& camera,
	const PostFX& post_fx
) {
	DrawShape(transform, rect, color, line_width, origin, depth, blend_mode, camera, post_fx);
}

void Renderer::DrawRoundedRect(
	const Transform& transform, const RoundedRect& rounded_rect, const Tint& color,
	const LineWidth& line_width, Origin origin, const Depth& depth, BlendMode blend_mode,
	const Camera& camera, const PostFX& post_fx
) {
	DrawShape(
		transform, rounded_rect, color, line_width, origin, depth, blend_mode, camera, post_fx
	);
}

void Renderer::DrawLine(
	const V2_float& start, const V2_float& end, const Tint& color, const LineWidth& line_width,
	const Depth& depth, BlendMode blend_mode, const Camera& camera, const PostFX& post_fx
) {
	DrawLine({}, Line{ start, end }, color, line_width, depth, blend_mode, camera, post_fx);
}

void Renderer::DrawLine(
	const Transform& transform, const Line& line, const Tint& color, const LineWidth& line_width,
	const Depth& depth, BlendMode blend_mode, const Camera& camera, const PostFX& post_fx
) {
	DrawShape(
		transform, line, color, line_width, Origin::Center, depth, blend_mode, camera, post_fx
	);
}

void Renderer::DrawTriangle(
	const Transform& transform, const Triangle& triangle, const Tint& color,
	const LineWidth& line_width, const Depth& depth, BlendMode blend_mode, const Camera& camera,
	const PostFX& post_fx
) {
	DrawShape(
		transform, triangle, color, line_width, Origin::Center, depth, blend_mode, camera, post_fx
	);
}

void Renderer::DrawEllipse(
	const Transform& transform, const Ellipse& ellipse, const Tint& color,
	const LineWidth& line_width, const Depth& depth, BlendMode blend_mode, const Camera& camera,
	const PostFX& post_fx
) {
	DrawShape(
		transform, ellipse, color, line_width, Origin::Center, depth, blend_mode, camera, post_fx
	);
}

void Renderer::DrawCircle(
	const Transform& transform, const Circle& circle, const Tint& color,
	const LineWidth& line_width, const Depth& depth, BlendMode blend_mode, const Camera& camera,
	const PostFX& post_fx
) {
	DrawShape(
		transform, circle, color, line_width, Origin::Center, depth, blend_mode, camera, post_fx
	);
}

void Renderer::DrawCapsule(
	const Transform& transform, const Capsule& capsule, const Tint& color,
	const LineWidth& line_width, const Depth& depth, BlendMode blend_mode, const Camera& camera,
	const PostFX& post_fx
) {
	DrawShape(
		transform, capsule, color, line_width, Origin::Center, depth, blend_mode, camera, post_fx
	);
}

void Renderer::DrawArc(
	const Transform& transform, const Arc& arc, const Tint& color, const LineWidth& line_width,
	const Depth& depth, BlendMode blend_mode, const Camera& camera, const PostFX& post_fx
) {
	DrawShape(
		transform, arc, color, line_width, Origin::Center, depth, blend_mode, camera, post_fx
	);
}

void Renderer::DrawPolygon(
	const Transform& transform, const Polygon& polygon, const Tint& color,
	const LineWidth& line_width, const Depth& depth, BlendMode blend_mode, const Camera& camera,
	const PostFX& post_fx
) {
	DrawShape(
		transform, polygon, color, line_width, Origin::Center, depth, blend_mode, camera, post_fx
	);
}

void Renderer::DrawPoint(
	const V2_float& point, const Tint& color, const Depth& depth, BlendMode blend_mode,
	const Camera& camera
) {
	DrawShape({}, point, color, -1.0f, Origin::Center, depth, blend_mode, camera, {});
}

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