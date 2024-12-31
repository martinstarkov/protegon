#include "renderer/renderer.h"

#include <functional>
#include <numeric>
#include <string_view>
#include <variant>

#include "scene/camera.h"
#include "core/window.h"
#include "frame_buffer.h"
#include "math/geometry/polygon.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "renderer/batch.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/font.h"
#include "renderer/frame_buffer.h"
#include "renderer/gl_renderer.h"
#include "renderer/origin.h"
#include "renderer/shader.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "utility/debug.h"
#include "utility/log.h"

namespace ptgn::impl {

void Renderer::Init() {
	GLRenderer::EnableLineSmoothing();
	GLRenderer::SetBlendMode(blend_mode_);

	batch_capacity_ = 2000;
	screen_			= FrameBuffer{ true };
	screen_.Bind();
	screen_.ClearToColor(clear_color_);

	auto get_indices = [](std::size_t max_indices, const auto& generator) {
		std::vector<std::uint32_t> indices;
		indices.resize(max_indices);
		std::generate(indices.begin(), indices.end(), generator);
		return indices;
	};

	constexpr std::array<std::uint32_t, 6> quad_index_pattern{ 0, 1, 2, 2, 3, 0 };

	auto quad_generator = [&quad_index_pattern, offset = 0, pattern_index = 0]() mutable {
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
	white_texture_ = ptgn::Texture({ color::White }, { 1, 1 });

	max_texture_slots_ = GLRenderer::GetMaxTextureSlots();

	quad_shader_   = game.shader.Get(PresetShader::Quad);
	circle_shader_ = game.shader.Get(PresetShader::Circle);
	color_shader_  = game.shader.Get(PresetShader::Color);

	PTGN_ASSERT(quad_shader_.IsValid());
	PTGN_ASSERT(circle_shader_.IsValid());
	PTGN_ASSERT(color_shader_.IsValid());

	std::vector<std::int32_t> samplers(max_texture_slots_);
	std::iota(samplers.begin(), samplers.end(), 0);

	quad_shader_.Bind();
	quad_shader_.SetUniform("u_Texture", samplers.data(), samplers.size());
}

void Renderer::Reset() {
	data_		 = {};
	clear_color_ = color::Transparent;
	blend_mode_	 = BlendMode::Blend;

	quad_shader_   = {};
	circle_shader_ = {};
	color_shader_  = {};

	quad_ib_	 = {};
	triangle_ib_ = {};
	line_ib_	 = {};
	point_ib_	 = {};
	shader_ib_	 = {};

	// Fade used with circle shader.
	batch_capacity_	   = 0;
	max_texture_slots_ = 0;
	white_texture_	   = {};
}

void Renderer::Shutdown() {
	Reset();
}

BlendMode Renderer::GetBlendMode() const {
	return blend_mode_;
}

void Renderer::SetBlendMode(BlendMode blend_mode) {
	if (blend_mode_ == blend_mode) {
		return;
	}
	blend_mode_ = blend_mode;
	GLRenderer::SetBlendMode(blend_mode_);
}

Color Renderer::GetClearColor() const {
	return clear_color_;
}

std::size_t Renderer::GetBatchCapacity() const {
	return batch_capacity_;
}

void Renderer::ResetRenderTarget() {
	screen_.Bind();
}

void Renderer::SetClearColor(const Color& color) {
	if (clear_color_ == color) {
		return;
	}
	clear_color_ = color;
	GLRenderer::ClearColor(clear_color_);
}

void Renderer::Clear() {
	GLRenderer::Clear();
}

void Renderer::Present() {
	game.renderer.Flush();
	if (bound_.IsValid()) {
		bound_.DrawToScreen();
	}

	FrameBuffer::Unbind();
	screen_.Draw({}, {}, {});

	game.window.SwapBuffers();

	// PTGN_LOG("Renderer Stats: \n", game.stats);
	// PTGN_LOG("--------------------------------------");
#ifdef PTGN_DEBUG
	game.stats.ResetRendererRelated();
#endif
}

void Renderer::Flush(std::size_t render_layer) {
	white_texture_.Bind(0);
	data_.FlushLayer(render_layer);
}

void Renderer::Flush() {
	white_texture_.Bind(0);
	data_.FlushLayers();
}

} // namespace ptgn::impl