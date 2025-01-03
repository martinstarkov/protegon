#include "renderer/renderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <vector>

#include "core/game.h"
#include "core/window.h"
#include "math/geometry/polygon.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/gl_renderer.h"
#include "renderer/layer_info.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/stats.h"

namespace ptgn::impl {

void Renderer::Init() {
	GLRenderer::EnableLineSmoothing();

	batch_capacity_ = 2000;

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

	screen_target_ = RenderTarget{ color::Transparent, BlendMode::Blend };

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

BlendMode Renderer::GetBlendMode() const {
	return game.scene.GetCurrent().GetTarget().GetBlendMode();
}

void Renderer::SetBlendMode(BlendMode blend_mode) {
	game.scene.GetCurrent().GetTarget().SetBlendMode(blend_mode);
}

Color Renderer::GetClearColor() const {
	return game.scene.GetCurrent().GetTarget().GetClearColor();
}

void Renderer::SetClearColor(const Color& clear_color) {
	game.scene.GetCurrent().GetTarget().SetClearColor(clear_color);
}

void Renderer::ClearScreen() const {
	FrameBuffer::Unbind();
	GLRenderer::ClearColor(color::Transparent);
	GLRenderer::SetBlendMode(BlendMode::Blend);
	GLRenderer::Clear();

	screen_target_.Clear();
}

void Renderer::Clear() const {
	game.scene.GetCurrent().GetTarget().Clear();
}

void Renderer::Flush() {
	game.scene.GetCurrent().GetTarget().Flush();
}

void Renderer::Present() {
	screen_target_.Flush();

	FrameBuffer::Unbind();
	screen_target_.GetTexture().Draw(Rect::Fullscreen(), {}, { 0, screen_target_ });
	screen_target_.Flush();

	game.window.SwapBuffers();

	// PTGN_LOG("Renderer Stats: \n", game.stats);
	// PTGN_LOG("--------------------------------------");
#ifdef PTGN_DEBUG
	game.stats.ResetRendererRelated();
#endif
}

} // namespace ptgn::impl