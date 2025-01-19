#include "renderer/renderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <vector>

#include "core/game.h"
#include "core/window.h"
#include "math/geometry/polygon.h"
#include "renderer/buffer.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/frame_buffer.h"
#include "renderer/gl_renderer.h"
#include "renderer/gl_types.h"
#include "renderer/layer_info.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "renderer/vertices.h"
#include "scene/scene_manager.h"
#include "utility/debug.h"
#include "utility/handle.h"

namespace ptgn::impl {

void Renderer::Init(const Color& background_color) {
	FrameBuffer::Unbind();
	GLRenderer::ClearColor(color::Transparent);
	GLRenderer::Clear();
	GLRenderer::SetBlendMode(BlendMode::None);
	// GLRenderer::EnableLineSmoothing();

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

	screen_target_ = RenderTarget{ background_color, BlendMode::BlendPremultiplied };

	quad_vao_	  = VertexArray{ PrimitiveMode::Triangles,
							 VertexBuffer{ static_cast<std::array<QuadVertex, 4>*>(nullptr),
										   batch_capacity_, BufferUsage::StreamDraw },
							 quad_vertex_layout, game.renderer.quad_ib_ };
	circle_vao_	  = VertexArray{ PrimitiveMode::Triangles,
								 VertexBuffer{ static_cast<std::array<CircleVertex, 4>*>(nullptr),
											   batch_capacity_, BufferUsage::StreamDraw },
								 circle_vertex_layout, game.renderer.quad_ib_ };
	triangle_vao_ = VertexArray{ PrimitiveMode::Triangles,
								 VertexBuffer{ static_cast<std::array<ColorVertex, 3>*>(nullptr),
											   batch_capacity_, BufferUsage::StreamDraw },
								 color_vertex_layout, game.renderer.triangle_ib_ };
	line_vao_	  = VertexArray{ PrimitiveMode::Lines,
							 VertexBuffer{ static_cast<std::array<ColorVertex, 2>*>(nullptr),
										   batch_capacity_, BufferUsage::StreamDraw },
							 color_vertex_layout, game.renderer.line_ib_ };
	point_vao_	  = VertexArray{ PrimitiveMode::Points,
								 VertexBuffer{ static_cast<std::array<ColorVertex, 1>*>(nullptr),
											   batch_capacity_, BufferUsage::StreamDraw },
								 color_vertex_layout, game.renderer.point_ib_ };

	ClearScreen();
}

void Renderer::Reset() {
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

	screen_target_ = {};
}

void Renderer::Shutdown() {
	Reset();
}

void Renderer::SetResolution(const V2_int& resolution) {
	resolution_ = resolution;
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
	return GetCurrentRenderTarget().GetBlendMode();
}

void Renderer::SetBlendMode(BlendMode blend_mode) {
	GetCurrentRenderTarget().SetBlendMode(blend_mode);
}

Color Renderer::GetClearColor() const {
	return GetCurrentRenderTarget().GetClearColor();
}

RenderTarget Renderer::GetCurrentRenderTarget() const {
	RenderTarget t;

	if (game.scene.HasCurrent()) {
		t = game.scene.GetCurrent().GetRenderTarget();
	} else {
		t = screen_target_;
	}

	PTGN_ASSERT(t.IsValid(), "Failed to find a current and valid render target");

	return t;
}

void Renderer::SetClearColor(const Color& clear_color) {
	GetCurrentRenderTarget().SetClearColor(clear_color);
}

void Renderer::ClearScreen() const {
	FrameBuffer::Unbind();
	GLRenderer::Clear();

	screen_target_.Clear();
}

void Renderer::Clear() const {
	GetCurrentRenderTarget().Clear();
}

void Renderer::Flush() {
	GetCurrentRenderTarget().Flush();
}

void Renderer::Present() {
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
			post_flush = [&]() {
				GLRenderer::SetViewport(
					letterbox_size, game.window.GetSize() - 2.0f * letterbox_size
				);
			};
			break;
		}
		case ResolutionMode::Overscan:	   break;
		case ResolutionMode::IntegerScale: break;
		default:						   PTGN_ERROR("Unrecognized resolution mode");
	}
	screen_target_.GetCamera().SetPrimary(camera);
	screen_target_.SetRect(dest);
	screen_target_.DrawToScreen(post_flush);

	game.window.SwapBuffers();

	// PTGN_LOG("Renderer Stats: \n", game.stats);
	// PTGN_LOG("--------------------------------------");
#ifdef PTGN_DEBUG
	game.stats.ResetRendererRelated();
#endif
}

} // namespace ptgn::impl