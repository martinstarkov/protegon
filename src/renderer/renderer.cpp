#include "renderer/renderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <numeric>
#include <type_traits>
#include <vector>

#include "core/game.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/buffer.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/gl_renderer.h"
#include "renderer/gl_types.h"
#include "renderer/render_data.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "renderer/vertices.h"
#include "scene/camera.h"
#include "utility/debug.h"
#include "utility/handle.h"

namespace ptgn::impl {

void Renderer::Init(const Color& window_background_color) {
	ClearScreen();
	GLRenderer::SetBlendMode(blend_mode_);
	// GLRenderer::EnableLineSmoothing();

	screen_target_ = RenderTarget{ window_background_color };
	active_target_ = screen_target_;

	batch_capacity_ = 2000;

	auto get_indices = [](std::size_t max_indices, const auto& generator) {
		std::vector<std::uint32_t> indices;
		indices.resize(max_indices);
		std::generate(indices.begin(), indices.end(), generator);
		return indices;
	};

	constexpr std::array<std::uint32_t, 6> quad_index_pattern{ 0, 1, 2, 2, 3, 0 };

	auto quad_generator = [&quad_index_pattern, offset = static_cast<std::uint32_t>(0),
						   pattern_index = static_cast<std::size_t>(0)]() mutable {
		auto index = offset + quad_index_pattern[pattern_index];
		pattern_index++;
		if (pattern_index % quad_index_pattern.size() == 0) {
			offset		  += 4;
			pattern_index  = 0;
		}
		return index;
	};

	auto iota = [i = 0]() mutable {
		return i++;
	};

	quad_ib_	 = { get_indices(batch_capacity_ * 6, quad_generator) };
	triangle_ib_ = { get_indices(batch_capacity_ * 3, iota) };
	line_ib_	 = { get_indices(batch_capacity_ * 2, iota) };
	point_ib_	 = { get_indices(batch_capacity_ * 1, iota) };
	shader_ib_	 = { std::array<std::uint32_t, 6>{ 0, 1, 2, 2, 3, 0 } };

	// First texture slot is occupied by white texture
	white_texture_ = Texture({ color::White }, { 1, 1 });

	max_texture_slots_ = GLRenderer::GetMaxTextureSlots();

	quad_shader_   = game.shader.Get(ShapeShader::Quad);
	circle_shader_ = game.shader.Get(ShapeShader::Circle);
	color_shader_  = game.shader.Get(ShapeShader::Color);

	PTGN_ASSERT(quad_shader_.IsValid());
	PTGN_ASSERT(circle_shader_.IsValid());
	PTGN_ASSERT(color_shader_.IsValid());

	std::vector<std::int32_t> samplers(static_cast<std::size_t>(max_texture_slots_));
	std::iota(samplers.begin(), samplers.end(), 0);

	quad_shader_.Bind();
	quad_shader_.SetUniform("u_Texture", samplers.data(), samplers.size());

	quad_vao_	  = VertexArray{ PrimitiveMode::Triangles,
							 VertexBuffer{ static_cast<std::array<QuadVertex, 4>*>(nullptr),
										   batch_capacity_, BufferUsage::StreamDraw },
							 quad_vertex_layout, quad_ib_ };
	circle_vao_	  = VertexArray{ PrimitiveMode::Triangles,
								 VertexBuffer{ static_cast<std::array<CircleVertex, 4>*>(nullptr),
											   batch_capacity_, BufferUsage::StreamDraw },
								 circle_vertex_layout, quad_ib_ };
	triangle_vao_ = VertexArray{ PrimitiveMode::Triangles,
								 VertexBuffer{ static_cast<std::array<ColorVertex, 3>*>(nullptr),
											   batch_capacity_, BufferUsage::StreamDraw },
								 color_vertex_layout, triangle_ib_ };
	line_vao_	  = VertexArray{ PrimitiveMode::Lines,
							 VertexBuffer{ static_cast<std::array<ColorVertex, 2>*>(nullptr),
										   batch_capacity_, BufferUsage::StreamDraw },
							 color_vertex_layout, line_ib_ };
	point_vao_	  = VertexArray{ PrimitiveMode::Points,
								 VertexBuffer{ static_cast<std::array<ColorVertex, 1>*>(nullptr),
											   batch_capacity_, BufferUsage::StreamDraw },
								 color_vertex_layout, point_ib_ };
}

void Renderer::Reset() {
	quad_vao_	  = {};
	circle_vao_	  = {};
	triangle_vao_ = {};
	line_vao_	  = {};
	point_vao_	  = {};

	quad_shader_   = {};
	circle_shader_ = {};
	color_shader_  = {};

	quad_ib_	 = {};
	triangle_ib_ = {};
	line_ib_	 = {};
	point_ib_	 = {};
	shader_ib_	 = {};

	batch_capacity_	   = 0;
	max_texture_slots_ = 0;
	white_texture_	   = {};
	resolution_		   = {};
	scaling_mode_	   = ResolutionMode::Disabled;
	render_data_	   = {};
	bound_shader_	   = {};
	blend_mode_		   = BlendMode::BlendPremultiplied;
	active_target_	   = {};
	screen_target_	   = {};

	FrameBuffer::Unbind();
}

void Renderer::Shutdown() {
	Reset();
}

void Renderer::SetResolution(const V2_int& resolution) {
	resolution_ = resolution;
	// User expects setting resolution to take effect immediately so it is defaulted to stretch.
	if (scaling_mode_ == ResolutionMode::Disabled) {
		scaling_mode_ = ResolutionMode::Stretch;
	}
}

void Renderer::SetResolutionMode(ResolutionMode scaling_mode) {
	scaling_mode_ = scaling_mode;
}

V2_int Renderer::GetResolution() const {
	if (resolution_.IsZero()) {
		return game.window.GetSize();
	}
	return resolution_;
}

ResolutionMode Renderer::GetResolutionMode() const {
	return scaling_mode_;
}

BlendMode Renderer::GetBlendMode() const {
	return blend_mode_;
}

void Renderer::SetBlendMode(BlendMode blend_mode) {
	if (blend_mode_ == blend_mode) {
		return;
	}
	Flush();
	blend_mode_ = blend_mode;
	GLRenderer::SetBlendMode(blend_mode_);
}

Color Renderer::GetClearColor() const {
	return active_target_.GetClearColor();
}

void Renderer::SetClearColor(const Color& clear_color) {
	active_target_.SetClearColor(clear_color);
}

void Renderer::SetRenderTarget(const RenderTarget& target) {
	if (active_target_ == target) {
		return;
	}
	Flush();
	if (target.IsValid()) {
		active_target_ = target;
	} else {
		active_target_ = screen_target_;
	}
	active_target_.SetCamera(game.camera.GetPrimary());
}

RenderTarget Renderer::GetRenderTarget() const {
	return active_target_;
}

impl::RenderData& Renderer::GetRenderData() {
	return render_data_;
}

void Renderer::ClearScreen() const {
	FrameBuffer::Unbind();
	GLRenderer::ClearColor(color::Transparent);
	GLRenderer::Clear();
}

void Renderer::SetTemporaryRenderTarget(
	const RenderTarget& render_target, const std::function<void()>& callback
) {
	PTGN_ASSERT(callback != nullptr, "Invalid callback given when setting temporary render target");
	const auto& previous_camera{ game.camera.GetPrimary() };
	auto previous_render_target{ GetRenderTarget() };
	auto previous_blend_mode{ GetBlendMode() };
	SetRenderTarget(render_target);
	std::invoke(callback);
	SetBlendMode(previous_blend_mode);
	SetRenderTarget(previous_render_target);
	game.camera.SetPrimary(previous_camera);
}

void Renderer::SetTemporaryBlendMode(BlendMode blend_mode, const std::function<void()>& callback) {
	PTGN_ASSERT(callback != nullptr, "Invalid callback given when setting temporary blend mode");
	auto previous_blend_mode{ GetBlendMode() };
	SetBlendMode(blend_mode);
	std::invoke(callback);
	SetBlendMode(previous_blend_mode);
}

void Renderer::Clear() const {
	active_target_.Bind();
	GLRenderer::ClearToColor(active_target_.GetClearColor());
}

void Renderer::Flush() {
	active_target_.Bind();

	if (render_data_.SetViewProjection(active_target_.GetCamera())) {
		// Post mouse event when camera projection changes.
		game.event.mouse.Post(MouseEvent::Move, MouseMoveEvent{});
	}

	render_data_.Flush();
}

void Renderer::SetViewport(const Rect& viewport) {
	active_target_.SetViewport(viewport);
}

Rect Renderer::GetViewport() const {
	return active_target_.GetViewport();
}

void Renderer::Present() {
	Flush();

	auto previous_render_target{ GetRenderTarget() };

	SetRenderTarget({});

	PTGN_ASSERT(active_target_ == screen_target_);

	screen_target_.Draw();

	FrameBuffer::Unbind();

	game.window.SwapBuffers();

	SetRenderTarget(previous_render_target);

	/*
	// TODO: Move this to happen only when setting resolution. This would allow for example only one
	// render target to be drawn as resolution.
	auto camera{ screen_target_.GetCamera().GetPrimary() };
	Rect dest{ Rect::Fullscreen() };
	auto center_on_resolution = [&]() {
		camera.CenterOnArea(resolution_.IsZero() ? game.window.GetSize() : resolution_);
	};
	std::function<void()> post_flush;
	switch (scaling_mode_) {
		case ResolutionMode::Disabled:
			camera.SetToWindow();
			// Uses fullscreen.
			// resolution_ = {};
			break;
		case ResolutionMode::Stretch:
			std::invoke(center_on_resolution);
			//   resolution_ = {};
			//   resolution_.origin = Origin::TopLeft;
			//   resolution_.size = resolution;
			break;
		case ResolutionMode::Letterbox: {
			// Size of the blackbars on one side.
			V2_float letterbox_size{ 160, 0 };
			V2_float size{ resolution_.IsZero() ? game.window.GetSize() : resolution_ };
			std::invoke(center_on_resolution);
			// camera.SetSize(size + letterbox_size);
			//  camera.SetPosition(size / 2.0f);
			GLRenderer::SetViewport(letterbox_size, game.window.GetSize() - 2.0f * letterbox_size);
			break;
		}
		case ResolutionMode::Overscan:	   break;
		case ResolutionMode::IntegerScale: break;
		default:						   PTGN_ERROR("Unrecognized resolution mode");
	}
	*/

	// PTGN_LOG("Renderer Stats: \n", game.stats);
	// PTGN_LOG("--------------------------------------");
#ifdef PTGN_DEBUG
	game.stats.ResetRendererRelated();
#endif
}

} // namespace ptgn::impl