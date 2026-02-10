#include "renderer/renderer.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/graphics/flip.h"
#include "core/log.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "platform/window/window.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_handle.h"
#include "renderer/backend/gl/gl_resource.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/resources/buffer_layout.h"
#include "renderer/resources/texture_format.h"
#include "renderer/resources/vertex.h"

namespace ptgn {

namespace impl {

std::array<V2_float, 4> GetTextureCoordinates(
	V2_float source_position, V2_float source_size, V2_float texture_size, bool offset_texels
) {
	PTGN_ASSERT(texture_size.x > 0.0f, "Texture must have width > 0");
	PTGN_ASSERT(texture_size.y > 0.0f, "Texture must have height > 0");

	PTGN_ASSERT(
		source_position.x < texture_size.x, "Source position X must be within texture width"
	);
	PTGN_ASSERT(
		source_position.y < texture_size.y, "Source position Y must be within texture height"
	);

	V2_float size{ source_size };

	if (size.IsZero()) {
		size = texture_size - source_position;
	}

	// Convert to 0 -> 1 range.
	V2_float src_pos{ source_position / texture_size };
	V2_float src_size{ size / texture_size };

	if (src_size.x > 1.0f || src_size.y > 1.0f) {
		PTGN_WARN("Drawing source size from outside of texture size");
	}

	V2_float half_pixel{ (offset_texels ? 0.5f : 0.0f) / texture_size };

	std::array<V2_float, 4> texture_coordinates{
		src_pos + half_pixel,
		V2_float{ src_pos.x + src_size.x - half_pixel.x, src_pos.y + half_pixel.y },
		src_pos + src_size - half_pixel,
		V2_float{ src_pos.x + half_pixel.x, src_pos.y + src_size.y - half_pixel.y },
	};

	return texture_coordinates;
}

void FlipTextureCoordinates(std::array<V2_float, 4>& texture_coords, Flip flip) {
	const auto flip_x = [&]() {
		std::swap(texture_coords[0].x, texture_coords[1].x);
		std::swap(texture_coords[2].x, texture_coords[3].x);
	};
	const auto flip_y = [&]() {
		std::swap(texture_coords[0].y, texture_coords[3].y);
		std::swap(texture_coords[1].y, texture_coords[2].y);
	};
	switch (flip) {
		case Flip::None:	   break;
		case Flip::Horizontal: flip_x(); break;
		case Flip::Vertical:   flip_y(); break;
		case Flip::Both:
			flip_x();
			flip_y();
			break;
		default: PTGN_ERROR("Unrecognized flip state");
	}
}

} // namespace impl

/*

bool Renderer::IsTextureAttachedToCurrentFramebuffer(
	Texture tex
) const {
	if (!state.framebuffer) {
		return false; // default framebuffer
	}

	const auto& fb = state.framebuffer;

	for (GLenum attachment : gl_->GetFramebufferColorAttachments(fb)) {
		const auto& info = gl_->GetFramebufferAttachment(fb, attachment);
		if (info.type == GL_TEXTURE_2D && info.id == tex) {
			return true;
		}
	}

	return false;
}

struct PingPong {
	Texture texture;
	Framebuffer fbo;
};

/// Key: texture ID, Value: PingPong struct containing the texture and its associated framebuffer.
std::unordered_map<GLuint, PingPong> ping_pong_cache;

PingPong& Renderer::GetPingPongFor(
	Texture src
) {
	auto& entry = ping_pong_cache[src.id];
	if (entry.texture) {
		return entry;
	}

	const auto& tex_info = gl_->GetTextureInfo(src);

	entry.texture = gl_->CreateTexture2D(
		tex_info.size,
		tex_info.internal_format,
		tex_info.filter,
		tex_info.wrap
	);

	entry.fbo = gl_->CreateFramebuffer();
	gl_->AttachTexture(entry.fbo, GL_COLOR_ATTACHMENT0, entry.texture);

	PTGN_ASSERT(gl_->CheckFramebufferComplete(entry.fbo));

	return entry;
}

void Renderer::ResolveReadWriteHazards() {
	if (!state.framebuffer) {
		return; // default framebuffer, no hazard
	}

	for (std::uint32_t i = 0; i < batch_textures.size(); ++i) {
		auto tex = batch_textures[i];

		if (!IsTextureAttachedToCurrentFramebuffer(tex)) {
			continue;
		}

		// Hazard detected
		auto& pp = GetPingPongFor(tex);

		// Flush pending geometry before redirecting
		FlushBatch();

		// Blit tex -> pingpong
		gl_->BlitTexture(tex, pp.texture);

		// Replace read texture in batch
		batch_textures[i] = pp.texture;

		// IMPORTANT: future writes now go to the pingpong target
		// so swap framebuffer attachment
		gl_->ReplaceFramebufferAttachment(
			state.framebuffer,
			tex,
			pp.texture
		);

		// Update state so next passes read the new texture
		std::swap(pp.texture, tex);

		break; // only need one resolve per flush
	}
}

void Renderer::FlushBatch() {
	if (batch_indices.empty()) {
		return;
	}

	ResolveReadWriteHazards();

	...
}


*/

//  TODO: Make ping pong system.
//  TODO: Make render target pooling system.
//  TODO: Make queued command system.
//  TODO: Make fork pipeline system.

/*
RecomputeDisplaySize(window_.GetSize());

// GLRenderer::EnableLineSmoothing();

GLRenderer::DisableDepthTesting();
GLRenderer::DisableGammaCorrection();

max_texture_slots = GLRenderer::GetMaxTextureSlots();

PTGN_INFO("Renderer Texture Slots: ", max_texture_slots);

const auto& screen_shader{ gl_->GetShader("screen_default") };
PTGN_ASSERT(screen_shader.IsValid());
gl_->Bind(screen_shader);
gl_->SetUniform(screen_shader, "u_Texture", 1);

const auto& quad_shader{ gl_->GetShader("quad") };

PTGN_ASSERT(quad_shader.IsValid());
PTGN_ASSERT(gl_->GetShader("circle").IsValid());
PTGN_ASSERT(gl_->GetShader("screen_default").IsValid());
PTGN_ASSERT(gl_->GetShader("light").IsValid());

intermediate_target = {};

screen_target_ = CreateRenderTarget(
	render_manager, display_viewport_.size, color::Transparent, TextureFormat::RGBA8888, true
);
AddScript<impl::DisplayResizeScript>(screen_target_);

SetBlendMode(screen_target_, BlendMode::ReplaceRGBA);

#ifdef PTGN_PLATFORM_MACOS
// Prevents MacOS warning: "UNSUPPORTED (log once): POSSIBLE ISSUE: unit X
// GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Float) - using zero
// texture because texture unloadable."
for (std::uint32_t slot{ 0 }; slot < max_texture_slots; slot++) {
	Texture::Bind(white_texture.GetId(), slot);
}
#endif

SetState(RenderState{ {}, BlendMode::ReplaceRGBA, {} });

viewport_tracker = render_manager.CreateEntity();
AddScript<ViewportResizeScript>(viewport_tracker, ctx_);
auto window_size{ window_.GetSize() };
RecomputeDisplaySize(window_size);

render_manager.Refresh();
*/

Renderer::Renderer(Window& window) :
	window_{ window }, gl_{ std::make_unique<impl::gl::GLContext>(window) } {
	ebo = gl_->CreateElementBuffer(
		nullptr, impl::index_capacity, sizeof(impl::Index), GL_DYNAMIC_DRAW
	);

	vbo = gl_->CreateVertexBuffer(
		nullptr, impl::vertex_capacity, sizeof(impl::Vertex), GL_DYNAMIC_DRAW
	);

	vao = gl_->CreateVertexArray(vbo, impl::Vertex::GetLayout(), ebo);

	white_texture = gl_->CreateTexture(
		static_cast<const void*>(&color::White), GL_RGBA, GL_UNSIGNED_INT, { 1, 1 }, GL_RGBA
	);

	auto window_size = window.GetSize();

	screen_target = CreateRenderTarget(window_size, TextureFormat::RGBA8);
	BindRenderTarget(screen_target);

	auto max_texture_slots{ gl_->GetMaxTextureSlots() };

	std::vector<std::int32_t> samplers(max_texture_slots);
	std::iota(samplers.begin(), samplers.end(), 0);

	auto quad{ gl_->GetShader("quad") };
	auto _1 = gl_->Bind(quad);
	gl_->SetUniform(
		quad, "u_Textures", samplers.data(), static_cast<std::int32_t>(samplers.size())
	);

#ifdef PTGN_PLATFORM_MACOS
	//  Prevents MacOS warning: "UNSUPPORTED (log once): POSSIBLE ISSUE: unit X
	//  GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Float) - using zero
	//  texture because texture unloadable."
	for (std::uint32_t slot{ 0 }; slot < max_texture_slots; slot++) {
		gl_->SetActiveTextureSlot(slot);
		auto _3 = gl_->Bind(white_texture);
	}
#endif
	gl_->SetActiveTextureSlot(0);
	auto _2 = gl_->Bind(white_texture);

	PTGN_ASSERT(batch_textures.empty());
	batch_textures.push_back(white_texture);
}

Renderer::~Renderer() noexcept {
	// Needs to have access to GLContext destructor, forward declaration is not enough.
}

RenderPass Renderer::BeginPass(RenderTarget scene_target) {
	RenderPass p{};
	p.source = scene_target;

	p.ping	   = AcquirePooledTarget(scene_target.size, scene_target.format);
	p.has_ping = true;

	p.has_written_once = false; // latest = source initially
	p.latest_is_ping   = true;	// irrelevant until has_written_once==true

	return p;
}

void Renderer::BindRenderTarget(RenderPass& p) {
	// Bind the next write target (opposite of latest output; ping for first write)
	RenderTarget write;

	if (!p.has_written_once) {
		write = p.ping;
	} else {
		if (!p.has_pong && p.latest_is_ping) {
			p.pong	   = AcquirePooledTarget(p.source.size, p.source.format);
			p.has_pong = true;
		}
		write = p.latest_is_ping ? p.pong : p.ping;
	}

	BindRenderTarget(write);
}

void Renderer::DrawTexture(impl::gl::ShaderId shader, RenderPass& p, RenderTarget scene_target) {
	// Input = latest output, or source before first draw
	RenderTarget input = !p.has_written_once ? p.source : p.latest_is_ping ? p.ping : p.pong;

	// Are we rendering *into this pass*?
	bool writing_to_pass = state.framebuffer == p.ping.framebuffer ||
						   (p.has_pong && state.framebuffer == p.pong.framebuffer);

	bool input_is_offscreen = input.framebuffer != scene_target.framebuffer;

	bool output_is_offscreen = state.framebuffer != scene_target.framebuffer;

	bool flip_y = input_is_offscreen && !output_is_offscreen;

	// Only ping-pong if we're writing into the pass
	if (writing_to_pass) {
		RenderTarget write;

		if (!p.has_written_once) {
			write = p.ping;
		} else {
			if (!p.has_pong && p.latest_is_ping) {
				p.pong	   = AcquirePooledTarget(p.source.size, p.source.format);
				p.has_pong = true;
			}
			write = p.latest_is_ping ? p.pong : p.ping;
		}

		BindRenderTarget(write);

		DrawTexturedQuad(
			shader, input.color, { 0, 0 }, gl_->GetTextureSize(input.color), color::White, flip_y
		);

		// Update pass state
		p.has_written_once = true;
		p.latest_is_ping   = (write.framebuffer == p.ping.framebuffer);
	} else {
		// Read-only draw: no mutation, no flip
		DrawTexturedQuad(
			shader, input.color, { 0, 0 }, gl_->GetTextureSize(input.color), color::White, flip_y
		);
	}
}

void Renderer::FlushBatch() {
	if (batch_indices.empty()) {
		return; // Nothing to draw
	}

	auto _vao = gl_->Bind(vao);

	// Upload vertex data
	gl_->SetBufferSubData<impl::gl::VertexBufferId>(
		vbo, GL_ARRAY_BUFFER, batch_vertices.data(), 0,
		static_cast<std::uint32_t>(batch_vertices.size()), sizeof(impl::Vertex)
	);

	// Upload index data
	gl_->SetBufferSubData<impl::gl::ElementBufferId>(
		ebo, GL_ELEMENT_ARRAY_BUFFER, batch_indices.data(), 0,
		static_cast<std::uint32_t>(batch_indices.size()), sizeof(impl::Index)
	);

	// Bind all textures
	for (std::uint32_t slot = 0; slot < batch_textures.size(); ++slot) {
		gl_->SetActiveTextureSlot(slot);
		auto _ = gl_->Bind(batch_textures[slot]);
	}

	// Draw
	gl_->DrawElements(
		vao, static_cast<std::uint32_t>(batch_indices.size()), GL_UNSIGNED_INT, GL_TRIANGLES
	);

	PTGN_LOG("Draw call");

	// Clear batch (keep white texture)
	batch_vertices.clear();
	batch_indices.clear();
	batch_textures.resize(1);
	batch_textures[0] = white_texture;
}

RenderTarget Renderer::AcquirePooledTarget(V2_int size, TextureFormat format) {
	++pool_tick;

	impl::PooledTarget* same_size = nullptr;
	impl::PooledTarget* unused	  = nullptr;
	impl::PooledTarget* oldest	  = nullptr;

	// 1. Spare with same size + format
	for (auto& e : rt_pool) {
		if (!e.in_use && e.target.size == size && e.target.format == format) {
			same_size = &e;
			break;
		}
	}

	// 2. Spare with same format, least recently used
	if (!same_size) {
		for (auto& e : rt_pool) {
			if (!e.in_use && e.target.format == format) {
				if (!unused || e.last_used_tick < unused->last_used_tick) {
					unused = &e;
				}
			}
		}
	}

	// 3. New target within pool limit
	if (!same_size && !unused && rt_pool.size() < max_pool_size) {
		impl::PooledTarget e{};
		e.target		 = CreateRenderTarget(size, format);
		e.in_use		 = true;
		e.last_used_tick = pool_tick;
		rt_pool.push_back(e);
		return e.target;
	}

	// 4. Oldest spare with same format
	if (!same_size && !unused) {
		for (auto& e : rt_pool) {
			if (!e.in_use && e.target.format == format) {
				if (!oldest || e.last_used_tick < oldest->last_used_tick) {
					oldest = &e;
				}
			}
		}
	}

	impl::PooledTarget* chosen = same_size ? same_size : unused ? unused : oldest;

	// 5. New target exceeding pool limit (no compatible spare)
	if (!chosen) {
		impl::PooledTarget e{};
		e.target		 = CreateRenderTarget(size, format);
		e.in_use		 = true;
		e.last_used_tick = pool_tick;
		rt_pool.push_back(e);
		return e.target;
	}

	// Resize if needed
	if (chosen->target.size != size) {
		ResizeRenderTarget(chosen->target, size);
	}

	chosen->in_use		   = true;
	chosen->last_used_tick = pool_tick;
	return chosen->target;
}

void Renderer::ReleasePooledTarget(RenderTarget target) {
	++pool_tick;

	for (auto& e : rt_pool) {
		if (e.target == target) {
			e.in_use		 = false;
			e.last_used_tick = pool_tick;
			return;
		}
	}
}

std::uint32_t Renderer::GetTextureSlot(impl::gl::TextureId tex) {
	if (tex == white_texture) {
		return 0; // always slot 0
	}

	// Check if texture already exists in batch
	for (std::uint32_t i = 1; i < batch_textures.size(); ++i) {
		if (batch_textures[i] == tex) {
			return i;
		}
	}

	// Get maximum texture slots from OpenGL
	std::uint32_t max_slots = static_cast<std::uint32_t>(gl_->GetMaxTextureSlots());

	// Flush if we would exceed GPU texture slots
	if (batch_textures.size() >= max_slots) {
		FlushBatch();
	}

	// Add texture to batch (but do NOT bind yet)
	batch_textures.push_back(tex);

	// Its slot is index in the vector
	return static_cast<std::uint32_t>(batch_textures.size() - 1);
}

void Renderer::SetShader(impl::gl::ShaderId shader) {
	if (!state.valid || state.shader != shader) {
		FlushBatch();
		auto _		 = gl_->Bind(shader);
		state.shader = shader;
		state.valid	 = true;
	}
}

void Renderer::SetBlend(BlendMode mode, bool enable) {
	if (!state.valid || state.blend_enable != enable || state.blend_mode != mode) {
		FlushBatch();

		state.blend_enable = enable;
		state.blend_mode   = mode;

		gl_->SetBlending(enable);

		if (enable) {
			gl_->SetBlendMode(mode);
		}

		state.valid = true;
	}
}

void Renderer::SetFramebuffer(
	impl::gl::FramebufferId framebuffer, const impl::gl::Viewport& viewport
) {
	if (!state.valid || state.framebuffer != framebuffer) {
		FlushBatch();
		auto _			  = gl_->Bind(framebuffer);
		state.framebuffer = framebuffer;
		state.valid		  = true;
	}

	gl_->SetViewport(viewport);
}

void Renderer::SetDepth(bool test, bool write, GLenum func) {
	if (!state.valid || state.depth_test != test || state.depth_write != write ||
		state.depth_func != func) {
		FlushBatch();

		state.depth_test  = test;
		state.depth_write = write;
		state.depth_func  = func;

		gl_->SetDepthTesting(test);

		if (test) {
			gl_->SetDepthFunc(func);
		}

		gl_->SetDepthMask(write);

		state.valid = true;
	}
}

void Renderer::SetStencil(
	bool enable, GLenum func, GLint ref, GLuint mask, GLenum fail, GLenum zfail, GLenum zpass,
	GLuint write_mask
) {
	if (!state.valid || state.stencil_test != enable || state.stencil_func != func ||
		state.stencil_ref != ref || state.stencil_mask != mask || state.stencil_fail != fail ||
		state.stencil_zfail != zfail || state.stencil_zpass != zpass ||
		state.stencil_write_mask != write_mask) {
		FlushBatch();

		state.stencil_test		 = enable;
		state.stencil_func		 = func;
		state.stencil_ref		 = ref;
		state.stencil_mask		 = mask;
		state.stencil_fail		 = fail;
		state.stencil_zfail		 = zfail;
		state.stencil_zpass		 = zpass;
		state.stencil_write_mask = write_mask;

		gl_->SetStencil(impl::gl::StencilState{
			.enabled	= state.stencil_test,
			.func		= state.stencil_func,
			.ref		= state.stencil_ref,
			.mask		= state.stencil_mask,
			.fail_op	= state.stencil_fail,
			.zfail_op	= state.stencil_zfail,
			.zpass_op	= state.stencil_zpass,
			.write_mask = state.stencil_write_mask,
		});

		state.valid = true;
	}
}

void Renderer::SetRaster(
	bool cull, GLenum cull_mode, GLenum front_face, GLenum polygon_front_mode,
	GLenum polygon_back_mode
) {
	if (!state.valid || state.cull_face != cull || state.cull_mode != cull_mode ||
		state.front_face != front_face || state.polygon_front_mode != polygon_front_mode ||
		state.polygon_back_mode != polygon_back_mode) {
		FlushBatch();

		state.cull_face			 = cull;
		state.cull_mode			 = cull_mode;
		state.front_face		 = front_face;
		state.polygon_front_mode = polygon_front_mode;
		state.polygon_back_mode	 = polygon_back_mode;

		gl_->SetCull(impl::gl::CullState{
			.enabled	= state.cull_face,
			.cull_face	= state.cull_mode,
			.front_face = state.front_face,
		});

		gl_->SetPolygonMode(polygon_front_mode, polygon_back_mode);

		state.valid = true;
	}
}

void Renderer::SetColorMask(bool r, bool g, bool b, bool a) {
	if (!state.valid || state.color_write_r != r || state.color_write_g != g ||
		state.color_write_b != b || state.color_write_a != a) {
		FlushBatch();

		state.color_write_r = r;
		state.color_write_g = g;
		state.color_write_b = b;
		state.color_write_a = a;

		gl_->SetColorMask(impl::gl::ColorMaskState{ .red = r, .green = g, .blue = b, .alpha = a });

		state.valid = true;
	}
}

static constexpr std::array<impl::Index, 6> MakeQuadIndices() {
	return { 0, 1, 2, 2, 3, 0 };
}

static std::array<V2_float, 4> MakeQuadPointsPixels(V2_float center, V2_float size) {
	const V2_float h{ size.x * 0.5f, size.y * 0.5f };

	return { center - h, center + V2_float{ h.x, -h.y }, center + h,
			 center + V2_float{ -h.x, h.y } };
}

static constexpr std::array<V2_float, 4> MakeTexCoords(bool flip_y) {
	if (!flip_y) {
		return { V2_float{ 0.0f, 0.0f }, V2_float{ 1.0f, 0.0f }, V2_float{ 1.0f, 1.0f },
				 V2_float{ 0.0f, 1.0f } };
	} else {
		return { V2_float{ 0.0f, 1.0f }, V2_float{ 1.0f, 1.0f }, V2_float{ 1.0f, 0.0f },
				 V2_float{ 0.0f, 0.0f } };
	}
}

impl::QuadDesc Renderer::MakeQuadDesc(const QuadParams& p) {
	impl::QuadDesc quad{};
	quad.positions	= MakeQuadPointsPixels(p.center, p.size);
	quad.tex_coords = p.tex_coords.value_or(MakeTexCoords(p.flip_y));
	quad.color		= p.tint;
	quad.rotation	= p.rotation;
	return quad;
}

void Renderer::DrawQuadEx(
	impl::gl::ShaderId shader, const QuadParams& params, const QuadSetup& setup
) {
	auto viewport		 = gl_->GetViewport();
	auto half_viewport	 = viewport.size * 0.5f;
	auto view_projection = Matrix4::Orthographic(-half_viewport, half_viewport);

	impl::QuadDesc quad = MakeQuadDesc(params);

	// Texture -> user data slot 0 (convention)
	if (params.texture) {
		std::uint32_t slot = GetTextureSlot(params.texture);
		quad.user_data[0]  = static_cast<float>(slot);
	}

	SetShader(shader);
	gl_->SetUniform(shader, "u_ViewProjection", view_projection);

	setup(shader, quad);

	auto vertices{ impl::Vertex::GetQuad(
		quad.positions, quad.color, quad.rotation, quad.user_data, quad.tex_coords
	) };
	auto indices{ MakeQuadIndices() };

	SubmitQuad(vertices, indices);
}

void Renderer::DrawQuadEx(
	impl::gl::ShaderId shader, const QuadParams& params, const UniformSetup& uniforms
) {
	DrawQuadEx(shader, params, [uniforms](impl::gl::ShaderId s, impl::QuadDesc&) {
		if (uniforms) {
			uniforms(s);
		}
	});
}

void Renderer::DrawTexturedQuad(
	impl::gl::ShaderId shader, impl::gl::TextureId texture, V2_float center, V2_float size,
	Color tint, bool flip_y
) {
	PTGN_ASSERT(
		impl::gl::TextureId{
			gl_->GetFramebufferAttachment(state.framebuffer, GL_COLOR_ATTACHMENT0).id } != texture,
		"Cannot draw a texture that is attached to the currently set framebuffer"
	);

	QuadParams p{};
	p.center  = center;
	p.size	  = size;
	p.tint	  = tint;
	p.texture = texture;
	p.flip_y  = flip_y;

	DrawQuadEx(shader, p, [this](auto s, auto& q) {
		gl_->SetUniform(s, "u_Texture", static_cast<std::int32_t>(q.user_data[0]));
	});
}

void Renderer::SubmitQuad(
	std::span<const impl::Vertex> vertices, std::span<const impl::Index> indices
) {
	if (batch_vertices.size() + vertices.size() >= MaxVertices ||
		batch_indices.size() + indices.size() >= MaxIndices) {
		FlushBatch();
	}

	auto start_index = static_cast<std::uint32_t>(batch_vertices.size());

	batch_vertices.insert(batch_vertices.end(), vertices.begin(), vertices.end());

	for (auto idx : indices) {
		batch_indices.push_back(idx + start_index);
	}
}

RenderTarget Renderer::CreateRenderTarget(V2_int size, TextureFormat format) const {
	const auto& desc = impl::gl::GetTextureFormatDesc(format);

	impl::gl::Texture color =
		gl_->CreateTexture(nullptr, desc.pixel_format, desc.pixel_type, size, desc.internal_format);

	impl::gl::Renderbuffer depth;
	if (desc.has_depth || desc.has_stencil) {
		GLenum rb_format = desc.has_stencil ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT;

		depth = gl_->CreateRenderbuffer(size, rb_format);
	}

	impl::gl::Framebuffer fb = gl_->CreateFramebuffer(
		color, GL_COLOR_ATTACHMENT0, depth,
		desc.has_stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT
	);

	return RenderTarget{
		.framebuffer = fb, .color = color, .depth = depth, .size = size, .format = format
	};
}

void Renderer::ResizeRenderTarget(RenderTarget& rt, V2_int new_size) const {
	if (rt.size == new_size) {
		return;
	}

	gl_->ResizeFramebuffer(rt.framebuffer, new_size);

	rt.size = new_size;
}

void Renderer::DrawTexture(impl::gl::TextureId texture, V2_float center, V2_float size) {
	QuadParams p{};
	p.center  = center;
	p.size	  = size;
	p.texture = texture;

	auto shader = gl_->GetShader("quad");

	DrawQuadEx(shader, p);
}

void Renderer::BeginFrame() {
	state.valid = false;
	PTGN_ASSERT(batch_vertices.empty());
	PTGN_ASSERT(batch_indices.empty());

	auto _1 = gl_->Bind(impl::gl::FramebufferId{});
	gl_->SetClearColor(color::Transparent);
	gl_->Clear();

	BindRenderTarget(screen_target);
	gl_->ClearToColor(screen_target.framebuffer, color::Transparent);
}

void Renderer::EndFrame() {
	auto window_size = window_.GetSize();

	SetFramebuffer({}, { { 0, 0 }, window_size });
	SetBlend(BlendMode::ReplaceRGBA);

	DrawTexture(screen_target.color, { 0, 0 }, screen_target.size);

	FlushBatch();
}

void Renderer::BindRenderTarget(
	impl::gl::FramebufferId framebuffer, const impl::gl::Viewport& viewport
) {
	SetFramebuffer(framebuffer, viewport);
}

void Renderer::BindRenderTarget(const RenderTarget& rt) {
	BindRenderTarget(rt.framebuffer, { { 0, 0 }, rt.size });
}

/*
void Renderer::DrawLightQuad(const LightParams& light) {
	QuadParams p{};
	p.center = light.position;
	p.size	 = { light.radius * 2.0f, light.radius * 2.0f };
	p.tint	 = color::White;

	auto shader = gl_->GetShader("light");

	DrawQuadEx(shader, p, [&](const auto& s, auto&) {
		gl_->SetUniform(s, "u_LightPosition", light.position);
		gl_->SetUniform(s, "u_Color", light.color.Normalized());
		gl_->SetUniform(s, "u_LightIntensity", light.intensity);
		gl_->SetUniform(s, "u_LightRadius", light.radius);
		gl_->SetUniform(s, "u_Falloff", light.falloff);
		gl_->SetUniform(s, "u_AmbientColor", light.ambient_color);
		gl_->SetUniform(s, "u_AmbientIntensity", light.ambient_intensity);
		gl_->SetUniform(s, "u_LightAttenuation", light.attenuation);
	});
}
*/

} // namespace ptgn

/*
ViewportResizeScript::ViewportResizeScript(Window& window, Renderer& renderer) :
	window{ window }, renderer{ renderer } {}

void ViewportResizeScript::OnWindowResized() {
	auto window_size{ window.GetSize() };
	if (!renderer.game_size_set_) {
		renderer.UpdateResolutions(window_size, renderer.resolution_mode_);
	}
	renderer.RecomputeDisplaySize(window_size);
}

static float GetFade(float diameter_y) {
	constexpr float fade_scaling_constant{ 0.12f };
	return fade_scaling_constant / diameter_y;
}

static float GetFade(V2_float diameter) {
	return GetFade(diameter.y);
}

static float NormalizeArcLineWidthToThickness(float line_width, float fade, V2_float radii) {
	if (line_width == -1.0f) {
		// Internally line width for a filled SDF is 1.0f.
		line_width = 1.0f;
	} else {
		PTGN_ASSERT(line_width >= min_line_width, "Invalid line width for circle");

		// Internally line width for a completely hollow ellipse is 0.0f.
		line_width = fade + line_width / std::min(radii.x, radii.y);
	}
	return line_width;
}

static float GetAspectRatio(V2_float size) {
	PTGN_ASSERT(size.x > 0.0f);
	return size.y / size.x;
}

static float GetNormalizedRadius(float diameter, float size_x) {
	PTGN_ASSERT(size_x > 0.0f);
	float normalized_radius{ diameter / size_x };
	return std::clamp(normalized_radius, 0.0f, 1.0f);
}

template <ShapeType T>
static std::array<float, 4> GetData(
	const T& shape, auto radius, float line_width, V2_float size
) {
	std::array<float, 4> data{ 0.0f, 0.0f, 0.0f, 0.0f };

	auto diameter{ 2.0f * radius };

	float fade{ GetFade(diameter) };

	float thickness{ NormalizeArcLineWidthToThickness(line_width, fade, V2_float{ radius }) };

	data[0] = thickness;
	data[1] = fade;

	if constexpr (std::is_same_v<T, Arc>) {
		float aperture{ shape.GetAperture() };
		float direction{ shape.clockwise ? 1.0f : -1.0f };

		data[2] = aperture;
		data[3] = direction;
	} else if constexpr (IsAnyOf<T, Capsule, RoundedRect>) {
		float normalized_radius{ GetNormalizedRadius(diameter, size.x) };
		float aspect_ratio{ GetAspectRatio(size) };

		data[2] = normalized_radius;
		data[3] = aspect_ratio;
	}

	return data;
}

struct QuadInfo {
	std::array<V2_float, 4> points;
	std::array<float, 4> data{ 0.0f, 0.0f, 0.0f, 0.0f };
};

template <ShapeType T>
static std::optional<QuadInfo> GetQuadInfo(Renderer& ctx, DrawShapeCommand& cmd, const T& shape) {
	QuadInfo info;

	const auto set_shader = [](DrawShapeCommand& c, std::string_view shader_name) {
		if (c.render_state.shader_pass.has_value() && *c.render_state.shader_pass != ShaderPass{}) {
			return;
		}
		c.render_state.shader_pass = Application::Get().shader.Get(shader_name);
	};

	if constexpr (std::is_same_v<T, V2_float>) {
		Transform translated = cmd.transform;
		translated.Translate(shape);

		Rect r{ V2_float{ 1.0f } };

		info.points = r.GetWorldVertices(translated, Origin::Center);
	} else if constexpr (std::is_same_v<T, Line>) {
		if (cmd.line_width < min_line_width) {
			return std::nullopt;
		}

		info.points = shape.GetWorldQuadVertices(cmd.transform, cmd.line_width);
	} else if constexpr (std::is_same_v<T, Capsule>) {
		auto radius{ shape.GetRadius(cmd.transform) };

		if (radius <= 0.0f) {
			return std::nullopt;
		}

		V2_float size;

		info.points = shape.GetWorldQuadVertices(cmd.transform, &size);
		info.data	= GetData(shape, radius, cmd.line_width, size);

		set_shader(cmd, "capsule");
	} else if constexpr (std::is_same_v<T, Arc>) {
		auto radius{ shape.GetRadius(cmd.transform) };

		if (radius <= 0.0f) {
			return std::nullopt;
		}

		Transform rotated{ cmd.transform };
		rotated.Rotate(shape.GetStartAngle());

		info.points = shape.GetWorldQuadVertices(rotated);
		info.data	= GetData(shape, radius, cmd.line_width, {});

		set_shader(cmd, "arc");
	} else if constexpr (std::is_same_v<T, RoundedRect>) {
		auto size = shape.GetSize(cmd.transform);

		if (!size.BothAboveZero()) {
			return std::nullopt;
		}

		float radius = shape.GetRadius(cmd.transform);

		if (radius <= 0.0f) {
			cmd.render_state.shader_pass = std::nullopt;
			cmd.shape					 = Rect{ shape.GetSize() };
			ctx.DrawCommand(cmd);
			return std::nullopt;
		}

		info.points = shape.GetWorldQuadVertices(cmd.transform, cmd.origin);
		info.data	= GetData(shape, radius, cmd.line_width, size);

		set_shader(cmd, "rounded_rect");
	} else if constexpr (std::is_same_v<T, Ellipse>) {
		auto radius = shape.GetRadius(cmd.transform);

		if (!radius.BothAboveZero()) {
			return std::nullopt;
		}

		info.points = shape.GetWorldQuadVertices(cmd.transform);
		info.data	= GetData(shape, radius, cmd.line_width, {});

		set_shader(cmd, "circle");
	} else {
		return std::nullopt;
	}

	return info;
}

template <ShapeType T>
static void DrawShape(Renderer& ctx, DrawShapeCommand cmd, const T& shape) {
	if constexpr (IsAnyOf<T, V2_float, Line, Capsule, Arc, RoundedRect, Ellipse>) {
		auto info{ GetQuadInfo(ctx, cmd, shape) };

		if (!info.has_value()) {
			return;
		}

		const auto& [points, data] = *info;

		auto quad_vertices{
			Vertex::GetQuad(points, cmd.tint, cmd.depth, data, GetDefaultTextureCoordinates())
		};

		ctx.SetState(cmd.render_state);
		ctx.AddVertices(quad_vertices, quad_indices);
	} else if constexpr (std::is_same_v<T, Circle>) {
		cmd.shape = Ellipse{ V2_float{ shape.GetRadius() } };
		ctx.DrawCommand(cmd);
	} else if constexpr (std::is_same_v<T, Rect>) {
		if (auto size{ shape.GetSize(cmd.transform) }; !size.BothAboveZero()) {
			return;
		}

		auto points = shape.GetWorldVertices(cmd.transform, cmd.origin);
		auto vertices =
			Vertex::GetQuad(points, cmd.tint, cmd.depth, { 0.0f }, GetDefaultTextureCoordinates());

		ctx.SetState(cmd.render_state);

		if (cmd.line_width == -1.0f) {
			ctx.AddVertices(vertices, quad_indices);
		} else {
			ctx.AddLinesImpl(vertices, quad_indices, points, cmd.line_width, {});
		}

	} else if constexpr (std::is_same_v<T, Triangle>) {
		auto points	  = shape.GetWorldVertices(cmd.transform);
		auto vertices = Vertex::GetTriangle(points, cmd.tint, cmd.depth);

		ctx.SetState(cmd.render_state);

		if (cmd.line_width == -1.0f) {
			ctx.AddVertices(vertices, triangle_indices);
		} else {
			ctx.AddLinesImpl(vertices, triangle_indices, points, cmd.line_width, {});
		}
	} else if constexpr (std::is_same_v<T, Polygon>) {
		ctx.SetState(cmd.render_state);

		if (shape.vertices.size() < 3) {
			if (shape.vertices.empty()) {
				return;
			} else if (shape.vertices.size() == 1) {
				cmd.shape = V2_float{ shape.vertices.front() };
				ctx.DrawCommand(cmd);
				return;
			} else if (shape.vertices.size() == 2) {
				cmd.shape = Line{ shape.vertices[0], shape.vertices[1] };
				ctx.DrawCommand(cmd);
				return;
			}
		}

		auto points = shape.GetWorldVertices(cmd.transform);

		if (cmd.line_width == -1.0f) {
			auto triangles{ Triangulate(points) };
			for (const auto& triangle : triangles) {
				auto vertices = Vertex::GetTriangle(triangle, cmd.tint, cmd.depth);
				ctx.AddVertices(vertices, triangle_indices);
			}
		} else {
			auto vertices =
				Vertex::GetQuad({}, cmd.tint, cmd.depth, { 0.0f }, GetDefaultTextureCoordinates());
			ctx.AddLinesImpl(vertices, quad_indices, points, cmd.line_width, {});
		}
	}
}

void Renderer::DrawCommand(const impl::DrawCommand& cmd) {
	std::visit(
		[&](const auto& command) {
			using T = std::decay_t<decltype(command)>;

			if constexpr (std::is_same_v<T, DrawShapeCommand>) {
				std::visit(
					[&](const auto& shape) { impl::DrawShape(*this, command, shape); },
					command.shape
				);
			} else if constexpr (std::is_same_v<T, DrawTextureCommand>) {
				DrawTexture(command);
			} else if constexpr (std::is_same_v<T, DrawShaderCommand>) {
				DrawShader(command);
			} else if constexpr (std::is_same_v<T, DrawLinesCommand>) {
				DrawLines(command);
			} else if constexpr (std::is_same_v<T, EnableStencilMask>) {
				Flush();
				StencilMask::Enable();
			} else if constexpr (std::is_same_v<T, DisableStencilMask>) {
				Flush();
				StencilMask::Disable();
			} else if constexpr (std::is_same_v<T, DrawInsideStencilMask>) {
				Flush();
				StencilMask::DrawInside();
			} else if constexpr (std::is_same_v<T, DrawOutsideStencilMask>) {
				Flush();
				StencilMask::DrawOutside();
			} else {
				PTGN_ERROR("Unknown draw command type");
			}
		},
		cmd
	);
}

void Renderer::DrawLines(const DrawLinesCommand& cmd) {
	std::size_t count = cmd.points.size();

	PTGN_ASSERT(cmd.line_width >= min_line_width);

	PTGN_ASSERT(
		(cmd.connect_last_to_first && count >= 3) || (!cmd.connect_last_to_first && count >= 2)
	);

	std::size_t vertex_modulo = count;
	if (!cmd.connect_last_to_first) {
		vertex_modulo -= 1;
	}

	SetState(cmd.render_state);

	for (std::size_t i = 0; i < count; ++i) {
		Line l{ cmd.points[i], cmd.points[(i + 1) % vertex_modulo] };
		auto quad_points   = l.GetWorldQuadVertices(cmd.transform, cmd.line_width);
		auto quad_vertices = Vertex::GetQuad(
			quad_points, cmd.tint, cmd.depth, { 0.0f }, GetDefaultTextureCoordinates()
		);
		AddVertices(quad_vertices, quad_indices);
	}
}

void Renderer::DrawTexture(const DrawTextureCommand& cmd) {
	auto texture_id{ cmd.texture_id };

	PTGN_ASSERT(texture_id, "Cannot draw textured quad with invalid texture");

	if (auto size{ cmd.rect.GetSize(cmd.transform) }; !size.BothAboveZero()) {
		return;
	}

	SetState(cmd.render_state);

	auto texture_points{ cmd.rect.GetWorldVertices(cmd.transform, cmd.origin) };

	auto texture_vertices{ Vertex::GetQuad(
		texture_points, cmd.tint, cmd.depth, { 0.0f }, cmd.texture_coordinates, false
	) };

	if (!cmd.pre_fx.pre_fx_.empty()) {
		PTGN_ASSERT(
			cmd.texture_size.BothAboveZero(),
			"Texture must have a valid size for it to have post fx"
		);

		Viewport viewport{ {}, cmd.texture_size };
		DrawTarget target;
		target.viewport		  = viewport;
		target.texture_format = cmd.texture_format;

		PTGN_ASSERT(target.viewport.size.BothAboveZero());

		auto half_viewport{ target.viewport.size * 0.5f };

		target.points = { target.viewport.position - half_viewport,
						  target.viewport.position + V2_float{ half_viewport.x, -half_viewport.y },
						  target.viewport.position + half_viewport,
						  target.viewport.position +
							  V2_float{ -half_viewport.x, half_viewport.y } };

		target.view_projection = Matrix4::Orthographic(target.points[0], target.points[2]);

		texture_id = PingPong(
			cmd.pre_fx.pre_fx_, draw_context_pool.Get(viewport.size, target.texture_format),
			texture_id, target, true
		);

		white_texture.Bind(0);
		force_flush = true;
	}

	float texture_index = 0.0f;

	auto get_texture_index = [&](TextureId id, float& out_texture_index) {
		PTGN_ASSERT(id != white_texture.GetId());
		// Texture exists in batch, therefore do not add it again.
		for (std::size_t i{ 0 }; i < textures_.size(); i++) {
			if (textures_[i] == id) {
				// i + 1 because first texture index is white texture.
				out_texture_index = static_cast<float>(i + 1);
				return true;
			}
		}
		// Batch is at texture capacity.
		if (static_cast<std::uint32_t>(textures_.size()) == max_texture_slots - 1) {
			Flush();
		}
		out_texture_index = static_cast<float>(textures_.size() + 1);
		return false;
	};

	bool existing = get_texture_index(texture_id, texture_index);

	Vertex::SetTextureIndex(texture_vertices, texture_index);
	AddVertices(texture_vertices, quad_indices);

	if (!existing) {
		// Must be done after AddVertices and SetState because both of them may Flush the
		// current batch, which will clear textures.
		textures_.emplace_back(texture_id);
	}

	PTGN_ASSERT(textures_.size() < max_texture_slots);
}

void Renderer::DrawShader(const DrawShaderCommand& cmd) {
	bool state_changed{ SetState(cmd.render_state) };

	bool uses_size{ std::holds_alternative<V2_int>(cmd.texture_or_size) };

	// Clear the intermediate frame buffer if the shader is new (changes renderer state), or if
	// the shader uses size (no texture) and the user desires it (most often true). In the case
	// of back-to-back light rendering this is not desired.
	bool clear{ state_changed || (uses_size && cmd.clear_between_consecutive_calls) };

	if (cmd.clear_between_consecutive_calls) {
		force_flush = true;
	}

	auto target{ drawing_to_ };

	if (render_state.camera) {
		target.view_projection = render_state.camera;
		target.points		   = render_state.camera.GetWorldVertices();
	}

	target.depth = cmd.depth;
	auto entity_tint{ cmd.entity ? GetTint(cmd.entity) : color::White };
	target.tint		  = target.tint.Normalized() * entity_tint.Normalized();
	target.blend_mode = cmd.intermediate_blend_mode;

	if (uses_size) {
		if (!std::get<V2_int>(cmd.texture_or_size).IsZero()) {
			target.viewport.size = std::get<V2_int>(cmd.texture_or_size);
		}
		target.texture_format = cmd.texture_format;
	} else if (std::holds_alternative<std::reference_wrapper<const Texture>>(cmd.texture_or_size)) {
		const Texture& texture =
			std::get<std::reference_wrapper<const Texture>>(cmd.texture_or_size).get();

		PTGN_ASSERT(texture.IsValid(), "Cannot draw shader to an invalid texture");

		target.viewport.size  = texture.GetSize();
		target.texture_id	  = texture.GetId();
		target.texture_format = texture.GetFormat();
	} else {
		PTGN_ERROR("Unknown variant value");
	}

	if (clear) {
		intermediate_target = draw_context_pool.Get(target.viewport.size, target.texture_format);
	}

	intermediate_target->blend_mode = cmd.target_blend_mode;

	PTGN_ASSERT(
		cmd.render_state.shader_pass.has_value(), "Must specify shader when drawing shader"
	);
	const auto& shader_pass = *cmd.render_state.shader_pass;
	const auto& shader		= shader_pass.GetShader();

	shader.Bind();
	shader.SetUniform("u_Texture", 1);
	shader.SetUniform("u_ViewportSize", V2_float{ target.viewport.size });

	if (shader_pass.uniform_callback) {
		PTGN_ASSERT(shader_pass.shader);
		gl_->Bind(shader_pass.shader);
		shader_pass.uniform_callback(cmd.entity, gl_, shader_pass.shader);
	}

	target.framebuffer = &intermediate_target->framebuffer;

	DrawCall(
		shader,
		Vertex::GetQuad(
			target.points, target.tint, target.depth, { 1.0f }, GetDefaultTextureCoordinates(),
			false
		),
		quad_indices, { target.texture_id }, target.framebuffer, clear, cmd.target_clear_color,
		target.blend_mode, target.viewport, target.view_projection
	);
}

TextureId Renderer::PingPong(
	const std::vector<Entity>& container, const std::shared_ptr<DrawContext>& read_context,
	TextureId id, DrawTarget target, bool flip_vertices
) {
	PTGN_ASSERT(!container.empty(), "Cannot ping pong on an empty container");

	auto read{ read_context };
	auto write{ draw_context_pool.Get(target.viewport.size, target.texture_format) };

	PTGN_ASSERT(read != nullptr && write != nullptr);
	PTGN_ASSERT(read->framebuffer.GetTexture().GetSize() == target.viewport.size);
	PTGN_ASSERT(write->framebuffer.GetTexture().GetSize() == target.viewport.size);

	bool use_previous_texture{ true };

	for (const auto& fx : container) {
		PTGN_ASSERT(fx.Has<ShaderPass>());

		bool first_effect{ fx == container.front() };

		if (!first_effect && use_previous_texture) {
			std::swap(read, write);
		}

		auto texture_id{ 0 };

		if ((first_effect || !use_previous_texture) && id) {
			texture_id = id;
		} else {
			texture_id = read->framebuffer.GetTexture().GetId();
		}

		const auto& shader_pass{ fx.Get<ShaderPass>() };
		const auto& shader{ shader_pass.shader };

		gl_->Bind(shader);
		gl_->SetUniform(shader, "u_Texture", 1);
		gl_->SetUniform(shader, "u_ViewportSize", V2_float{ target.viewport.size });

		if (shader_pass.uniform_callback) {
			PTGN_ASSERT(shader_pass.shader);
			gl_->Bind(shader_pass.shader);
			shader_pass.uniform_callback(fx, gl_, shader_pass.shader);
		}

		target.texture_id	= texture_id;
		target.framebuffer = &write->framebuffer;
		target.tint			= GetTint(fx);
		target.blend_mode	= GetBlendMode(fx);

		DrawCall(
			shader,
			Vertex::GetQuad(
				target.points, target.tint, target.depth, { 1.0f }, GetDefaultTextureCoordinates(),
				flip_vertices
			),
			quad_indices, { target.texture_id }, target.framebuffer, use_previous_texture,
			color::Transparent, target.blend_mode, target.viewport, target.view_projection
		);

		use_previous_texture = fx.GetOrDefault<UsePreviousTexture>();
	}
	read->in_use = false;

	return write->framebuffer.GetTexture().GetId();
}

void Renderer::AddTemporaryTexture(Texture&& texture) {
	temporary_textures.emplace_back(std::move(texture));
}

std::size_t Renderer::GetMaxTextureSlots() const {
	if (!max_texture_slots) {
		max_texture_slots = GLRenderer::GetMaxTextureSlots();
	}
	return max_texture_slots;
}

void Renderer::AddLinesImpl(
	std::span<Vertex> line_vertices, std::span<const Index> line_indices,
	std::span<const V2_float> points, float line_width, const Transform& transform
) {
	PTGN_ASSERT(line_width >= min_line_width, "Invalid line width for lines");

	for (std::size_t i = 0; i < points.size(); ++i) {
		Line l{ points[i], points[(i + 1) % points.size()] };
		auto line_points{ l.GetWorldQuadVertices(transform, line_width) };

		PTGN_ASSERT(line_vertices.size() <= line_points.size());

		for (std::size_t j = 0; j < line_vertices.size(); ++j) {
			line_vertices[j].position[0] = line_points[j].x;
			line_vertices[j].position[1] = line_points[j].y;
		}

		AddVertices(line_vertices, line_indices);
	}
}

void Renderer::AddVertices(
	std::span<const Vertex> point_vertices, std::span<const Index> point_indices
) {
	if (vertices_.size() + point_vertices.size() > vertex_capacity ||
		indices_.size() + point_indices.size() > index_capacity) {
		Flush();
	}

	vertices_.insert(vertices_.end(), point_vertices.begin(), point_vertices.end());

	indices_.reserve(indices_.size() + point_indices.size());

	for (auto index : point_indices) {
		indices_.emplace_back(index + index_offset_);
	}

	index_offset_ += static_cast<Index>(point_vertices.size());
}

void Renderer::DrawCall(
	const Shader& shader, std::span<const Vertex> vertices, std::span<const Index> indices,
	const std::vector<Handle<Texture>>& textures, const Framebuffer* framebuffer,
	bool clear_framebuffer, Color clear_color, BlendMode blend_mode,
	const Viewport& viewport, const Matrix4& view_projection
) {
	if (vertices.empty() || indices.empty()) {
		return;
	}

	if (framebuffer) {
		framebuffer->Bind();
	} else {
		Framebuffer::Unbind();
	}

	if (clear_framebuffer) {
		GLRenderer::ClearToColor(clear_color);
	}

	PTGN_ASSERT(viewport.size.BothAboveZero(), "Viewport size must be above zero");

	GLRenderer::SetViewport(viewport.position, viewport.size);
	GLRenderer::SetBlendMode(blend_mode);

	triangle_vao.Bind();

	triangle_vao.GetVertexBuffer().SetSubData(
		vertices.data(), 0, static_cast<std::uint32_t>(vertices.size()), sizeof(Vertex), false, true
	);

	triangle_vao.GetIndexBuffer().SetSubData(
		indices.data(), 0, static_cast<std::uint32_t>(indices.size()), sizeof(Index), false, true
	);

	shader.Bind();
	shader.SetUniform("u_ViewProjection", view_projection);

	PTGN_ASSERT(textures.size() < max_texture_slots);

	for (std::uint32_t i{ 0 }; i < static_cast<std::uint32_t>(textures.size()); i++) {
		PTGN_ASSERT(textures[i], "Cannot bind invalid texture");
		// Save first texture slot for empty white texture.
		std::uint32_t slot{ i + 1 };
		gl_->SetActiveTextureSlot(slot);
		gl_->Bind(textures[i]);
	}

	gl_->DrawElements(triangle_vao, indices.size(), GL_TRIANGLES);
}

void Renderer::Flush(bool final_flush) {
	std::vector<TextureId> texture_id;

	bool has_post_fx{ !render_state.post_fx.post_fx_.empty() };

	auto target{ drawing_to_ };

	if (render_state.IsSet()) {
		if (render_state.camera) {
			target.view_projection = render_state.camera;
			target.points		   = render_state.camera.GetWorldVertices();
		}
		target.blend_mode = render_state.blend_mode;
	}

	if (has_post_fx) {
		PTGN_ASSERT(!intermediate_target);

		intermediate_target = draw_context_pool.Get(target.viewport.size, target.texture_format);

		target.framebuffer = &intermediate_target->framebuffer;

		const auto& shader{ GetCurrentShader() };

		// Draw unflushed vertices to intermediate target before adding post fx to it.
		DrawCall(
			shader, vertices_, indices_, textures_, target.framebuffer, true, color::Transparent,
			target.blend_mode, target.viewport, target.view_projection
		);

		// Add post fx to the intermediate target.

		// Flip only every odd ping pong to keep the flushed target upright.
		bool flip{ render_state.post_fx.post_fx_.size() % 2 == 1 };
		auto id{ PingPong(render_state.post_fx.post_fx_, intermediate_target, {}, target, flip) };
		target.texture_id = id;
	}

	// Reset because post fx may change target.framebuffer.
	target.framebuffer = drawing_to_.framebuffer;

	if (intermediate_target) {
		// This branch is for when an intermediate target needs to be flushed onto the
		// drawing_to frame buffer. It is used in cases where postfx are applied, or when a
		// shader that uses the intermediate target is being flushed (for instance a set of
		// lights rendered onto an intermediate target and then flushed onto the drawing_to
		// frame buffer).

		if (!has_post_fx) {
			// The light case discussed above.
			const auto& texture{ intermediate_target->framebuffer.GetTexture() };
			target.texture_id	  = texture.GetId();
			target.texture_format = texture.GetFormat();
			target.texture_size	  = texture.GetSize();
		}
		if (intermediate_target->blend_mode.has_value()) {
			target.blend_mode = *intermediate_target->blend_mode;
		}

		// Only flip if postfx have been applied.
		DrawCall(
			GetFullscreenShader(target.texture_format),
			Vertex::GetQuad(
				target.points, target.tint, target.depth, { 1.0f }, GetDefaultTextureCoordinates(),
				has_post_fx
			),
			quad_indices, { target.texture_id }, target.framebuffer, false, color::Transparent,
			target.blend_mode, target.viewport, target.view_projection
		);

	} else if (render_state.IsSet()) {
		// No post fx, and no intermediate target.

		const auto& shader{ GetCurrentShader() };

		// Draw unflushed vertices directly to drawing_to frame buffer.
		DrawCall(
			shader, vertices_, indices_, textures_, target.framebuffer, false, color::Transparent,
			target.blend_mode, target.viewport, target.view_projection
		);
	}

	Reset();

	if (final_flush) {
		render_state = {};
	}
}

void Renderer::Reset() {
	intermediate_target = {};
	vertices_.clear();
	indices_.clear();
	textures_.clear();
	index_offset_ = 0;
	force_flush	  = false;
	draw_context_pool.TrimExpired();
}

void Renderer::InvokeDrawable(const Entity& entity) {
	PTGN_ASSERT(entity.Has<IDrawable>(), "Cannot render entity without drawable component");

	const auto& drawable{ entity.GetImpl<IDrawable>() };

	const auto& drawable_functions{ IDrawable::data() };

	PTGN_ASSERT(drawable_functions.contains(drawable.hash), "Failed to identify drawable hash");

	const auto& draw_function{ drawable_functions.find(drawable.hash)->second };

	draw_function(entity);
}

void Renderer::InvokeDrawFilter(RenderTarget& render_target, FilterType type) {
	if (!render_target.Has<IDrawFilter>()) {
		return;
	}

	const auto& filter{ render_target.GetImpl<IDrawFilter>() };
	const auto& filter_functions{ IDrawFilter::data() };

	PTGN_ASSERT(filter_functions.contains(filter.hash), "Failed to identify filter hash");

	const auto& filter_function{ filter_functions.find(filter.hash)->second };

	PTGN_ASSERT(filter_function);

	filter_function(render_target, type);
}

void Renderer::FlushDrawQueue(TextureId id, bool draw_debug) {
	auto it{ draw_queues_.find(id) };

	if (it != draw_queues_.end()) {
		std::vector<impl::DrawCommand>& commands{ it->second };

		for (const auto& command : commands) {
			DrawCommand(command);
		}
	}

	if (draw_debug) {
		for (const auto& command : debug_queue_) {
			DrawCommand(command);
		}
	}

	Flush(true);
}

void Renderer::DrawDisplayList(
	RenderTarget& render_target, std::vector<Entity>& display_list,
	const std::function<bool(const Entity&)>& filter, bool draw_debug
) {
	Camera camera{ render_target.GetCamera() };

	const auto& texture{ render_target.GetTexture() };
	auto texture_size{ render_target.GetTextureSize() };

	drawing_to_.texture_size	  = texture_size;
	drawing_to_.texture_id		  = texture.GetId();
	drawing_to_.texture_format	  = texture.GetFormat();
	drawing_to_.viewport.position = {};
	drawing_to_.viewport.size	  = texture_size;

	drawing_to_.view_projection = camera;
	drawing_to_.points			= camera.GetWorldVertices();

	drawing_to_.blend_mode	 = GetBlendMode(render_target);
	drawing_to_.depth		 = GetDepth(render_target);
	drawing_to_.tint		 = GetTint(render_target);
	drawing_to_.framebuffer = &render_target.GetFramebuffer();

	// Must be sorted here so that depth and creation order is accounted for.
	SortByDepth(display_list, true);

	InvokeDrawFilter(render_target, FilterType::Pre);

	for (const auto& entity : display_list) {
		if (filter && filter(entity)) {
			continue;
		}
		InvokeDrawable(entity);
	}

	InvokeDrawFilter(render_target, FilterType::Post);

	FlushDrawQueue(drawing_to_.texture_id, draw_debug);
}

void Renderer::SetDrawingTo(const RenderTarget& render_target) {
	const auto& texture{ render_target.GetTexture() };
	auto texture_size{ render_target.GetTextureSize() };
	Camera camera{ render_target.GetCamera() };

	drawing_to_.texture_size	  = texture_size;
	drawing_to_.texture_id		  = texture.GetId();
	drawing_to_.texture_format	  = texture.GetFormat();
	drawing_to_.viewport.position = {};
	drawing_to_.viewport.size	  = texture_size;

	drawing_to_.view_projection = camera;
	drawing_to_.points			= camera.GetWorldVertices();

	drawing_to_.blend_mode	 = GetBlendMode(render_target);
	drawing_to_.depth		 = GetDepth(render_target);
	drawing_to_.tint		 = GetTint(render_target);
	drawing_to_.framebuffer = &render_target.GetFramebuffer();
}

void Renderer::DrawScene(Scene& scene) {
	// Loop through render targets and render their display lists onto their internal frame
	// buffers.
	for (auto [entity, visible, drawable, framebuffer, display_list] :
		 scene.InternalEntitiesWith<Visible, IDrawable, Framebuffer, DisplayList>()) {
		if (!visible) {
			continue;
		}

		RenderTarget rt{ entity };

		DrawDisplayList(rt, display_list.entities);
	}

	auto& display_list{ scene.render_target_.GetDisplayList() };

	DrawDisplayList(
		scene.render_target_, display_list,
		[](const Entity& entity) {
			// Skip entities which are in the display list of a custom render target.
			return entity.Has<RenderTarget>();
		},
		true
	);
}

void Renderer::RecomputeDisplaySize(V2_int window_size) {
	if (!game_size_.BothAboveZero()) {
		UpdateResolutions(window_size, resolution_mode_);
	}

	Viewport vp;
	vp.position = { 0, 0 };
	vp.size		= window_size;

	auto compute_aspect_fit = [&](bool letterbox_mode) {
		float window_aspect{ static_cast<float>(window_size.x) /
							 static_cast<float>(window_size.y) };
		float game_aspect{ static_cast<float>(game_size_.x) / static_cast<float>(game_size_.y) };

		// In letterbox mode we need require window_aspect > game_aspect to fit height, and in
		// overscan we require window_aspect > game_aspect to fit height.
		bool fit_height{ (window_aspect > game_aspect) == letterbox_mode };

		if (fit_height) {
			vp.size.y = window_size.y;
			vp.size.x = static_cast<int>(static_cast<float>(window_size.y) * game_aspect + 0.5f);
			vp.position.x = (window_size.x - vp.size.x) / 2; // left edge.
			vp.position.y = 0;
		} else {
			// Fit width.
			vp.size.x = window_size.x;
			vp.size.y = static_cast<int>(static_cast<float>(window_size.x) / game_aspect + 0.5f);
			vp.position.x = 0;
			vp.position.y = (window_size.y - vp.size.y) / 2; // top edge.
		}
	};

	switch (resolution_mode_) {
		case ScalingMode::Letterbox:	compute_aspect_fit(true); break;

		case ScalingMode::IntegerScale: {
			V2_int ratio{ window_size / game_size_ };
			// Find which dimension limits the scaling factor.
			int scale{ std::max(1, std::min(ratio.x, ratio.y)) };
			vp.size		= game_size_ * scale;		   // scale up.
			vp.position = (window_size - vp.size) / 2; // center of window.
			break;
		}

		case ScalingMode::Stretch:
			// Viewport is full window (default).
			break;

		case ScalingMode::Disabled:
			vp.size		= game_size_;				   // no change.
			vp.position = (window_size - vp.size) / 2; // center of window.
			break;

		case ScalingMode::Overscan: compute_aspect_fit(false); break;

		default:					PTGN_ERROR("Unsupported resolution mode");
	}

	if (vp != display_viewport_) {
		// Only update viewport if it changed. This reduces DisplaySizeChanged event
		// dispatch.
		display_viewport_	  = vp;
		display_size_changed_ = true;
	}
}

void Renderer::UpdateResolutions(V2_int game_size, ScalingMode scaling_mode) {
	bool new_game_size{ game_size_ != game_size };
	if (!new_game_size && resolution_mode_ == scaling_mode) {
		return;
	}
	auto window_size{ window_.GetSize() };
	game_size_		   = game_size;
	resolution_mode_   = scaling_mode;
	game_size_changed_ = new_game_size;
	RecomputeDisplaySize(window_size);
}

void Renderer::ClearScreenTarget() const {
	screen_target_.Clear();
}

void Renderer::ClearRenderTargets(Scene& scene) const {
	scene.render_target_.Clear();

	for (auto [entity, framebuffer] : scene.EntitiesWith<Framebuffer>()) {
		RenderTarget rt{ entity };
		rt.Clear();
		// rt.ClearDisplayList();
	}
}

void Renderer::DrawScreenTarget() {
	auto half_viewport{ display_viewport_.size * 0.5f };

	const auto& texture{ screen_target_.GetTexture() };

	DrawCall(
		GetFullscreenShader(texture.GetFormat()),
		Vertex::GetQuad(
			{ -half_viewport, V2_float{ half_viewport.x, -half_viewport.y }, half_viewport,
			  V2_float{ -half_viewport.x, half_viewport.y } },
			GetTint(screen_target_), GetDepth(screen_target_), { 1.0f },
			GetDefaultTextureCoordinates(), true
		),
		quad_indices, { texture.GetId() }, nullptr, false, color::Transparent,
		GetBlendMode(screen_target_), display_viewport_,
		Matrix4::Orthographic(-half_viewport, half_viewport)
	);
}

void Renderer::Draw(Scene& scene) {
	// PTGN_LOG(draw_context_pool.contexts_.size());
	// PTGN_PROFILE_FUNCTION();

	white_texture.Bind(0);

	DrawScene(scene);

	auto half_game_size{ game_size_ * 0.5f };

	Transform scene_transform{ GetTransform(scene.render_target_) };

	auto points{ Rect{ scene.camera.GetViewportSize() }.GetWorldVertices(scene_transform) };
	auto projection{ Matrix4::Orthographic(-half_game_size, half_game_size) };

	Viewport viewport{ {}, display_viewport_.size };

	const auto& texture{ scene.render_target_.GetTexture() };

	DrawCall(
		GetFullscreenShader(texture.GetFormat()),
		Vertex::GetQuad(
			points, GetTint(scene.render_target_), GetDepth(scene.render_target_), { 1.0f },
			GetDefaultTextureCoordinates(), true
		),
		quad_indices, { texture.GetId() }, &screen_target_.GetFramebuffer(), false,
		color::Transparent, GetBlendMode(scene.render_target_), viewport, projection
	);

	draw_queues_.clear();
	debug_queue_.clear();

	Reset();

	render_state	   = {};
	temporary_textures = std::vector<Texture>{};
}

} // namespace impl

// TODO: Get rid of this.
using namespace impl;

void Renderer::DrawTexture(
	const Texture& texture, const Transform& transform, V2_float texture_size, Origin origin,
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
	Color target_clear_color, const TextureOrSize& texture_or_size,
	BlendMode intermediate_blend_mode, const Depth& depth, BlendMode blend_mode,
	const Camera& camera, TextureFormat texture_format, const PostFX& post_fx,
	std::optional<BlendMode> target_blend_mode
) {
	DrawShaderCommand cmd;

	cmd.entity							= entity;
	cmd.clear_between_consecutive_calls = clear_between_consecutive_calls;
	cmd.target_clear_color				= target_clear_color;
	cmd.texture_or_size					= texture_or_size;
	cmd.intermediate_blend_mode			= intermediate_blend_mode;
	cmd.target_blend_mode				= target_blend_mode;
	cmd.depth							= depth;
	cmd.texture_format					= texture_format;
	cmd.render_state.shader_pass		= shader_pass;
	cmd.render_state.post_fx			= post_fx;
	cmd.render_state.blend_mode			= blend_mode;
	cmd.render_state.camera				= camera;

	render_data_.Submit(cmd);
}

impl::Texture Renderer::CreateTexture(
	Transform& out_transform, V2_float& out_text_size, const TextContent& content,
	const TextColor& color, const FontSize& font_size, const ResourceHandle& font_key,
	const TextProperties& properties, bool hd_text, const Camera& camera
) {
	FontSize final_font_size{ font_size };

	if (hd_text) {
		// TODO: Figure out a better solution to this.
		const auto& scene{ ctx_->scene->GetCurrent() };

		auto render_target_scale{ scene->GetRenderTargetScaleRelativeTo(camera) };

		PTGN_ASSERT(render_target_scale.BothAboveZero());

		out_transform.Scale(1.0f / render_target_scale);

		final_font_size =
			static_cast<std::int32_t>(static_cast<float>(font_size) * render_target_scale.y);
	}

	auto texture{ Text::CreateTexture(content, color, final_font_size, font_key, properties) };

	if (out_text_size.IsZero()) {
		out_text_size = Text::GetSize(content, font_key, final_font_size);
	}

	return texture;
}

void Renderer::DrawText(
	const std::string& content, Transform transform, const TextColor& color, Origin origin,
	const FontSize& font_size, const ResourceHandle& font_key, const TextProperties& properties,
	V2_float text_size, const Tint& tint, bool hd_text, const Depth& depth, BlendMode blend_mode,
	const Camera& camera, const PreFX& pre_fx, const PostFX& post_fx,
	const std::array<V2_float, 4>& texture_coordinates
) {
	auto texture{ CreateTexture(
		transform, text_size, content, color, font_size, font_key, properties, hd_text, camera
	) };

	DrawTexture(
		texture, transform, text_size, origin, tint, depth, blend_mode, camera, pre_fx, post_fx,
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
	V2_float start, V2_float end, const Tint& color, const LineWidth& line_width,
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
	V2_float point, const Tint& color, const Depth& depth, BlendMode blend_mode,
	const Camera& camera
) {
	DrawShape({}, point, color, -1.0f, Origin::Center, depth, blend_mode, camera, {});
}

void Renderer::EnableStencilMask() {
	render_data_.Submit(impl::EnableStencilMask{});
}

void Renderer::DisableStencilMask() {
	render_data_.Submit(impl::DisableStencilMask{});
}

void Renderer::DrawOutsideStencilMask() {
	render_data_.Submit(impl::DrawOutsideStencilMask{});
}

void Renderer::DrawInsideStencilMask() {
	render_data_.Submit(impl::DrawInsideStencilMask{});
}

void Renderer::SetBackgroundColor(Color background_color) {
	render_data_.screen_target_.SetClearColor(background_color);
}

Color Renderer::GetBackgroundColor() const {
	return render_data_.screen_target_.GetClearColor();
}

void Renderer::SetScalingMode(ScalingMode scaling_mode) {
	V2_int resolution{ render_data_.game_size_set_ ? render_data_.game_size_ : window_.GetSize() };
	render_data_.UpdateResolutions(resolution, scaling_mode);
}

void Renderer::SetGameSize(V2_int game_size, ScalingMode scaling_mode) {
	render_data_.game_size_set_ = !game_size.IsZero();
	V2_int resolution{ render_data_.game_size_set_ ? game_size : window_.GetSize() };
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
	Framebuffer::Unbind();

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
		Framebuffer::IsUnbound(),
		"Frame buffer must be unbound (id=0) before swapping SDL buffer to the screen"
	);

	window->SwapBuffers();
}

void Renderer::ClearScreen() const {
	Framebuffer::Unbind();
	GLRenderer::SetClearColor(color::Transparent);
	GLRenderer::Clear();
	render_data_.ClearScreenTarget();
}

*/