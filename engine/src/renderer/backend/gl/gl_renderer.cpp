#include "renderer/backend/gl/gl_renderer.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "platform/window/window.h"
#include "renderer/backend/gl/gl_buffer.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_shader.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/backend/gl/gl_vertex_array.h"
#include "renderer/camera/viewport.h"
#include "renderer/resources/buffer_layout.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/render_state.h"
#include "renderer/resources/render_target.h"
#include "renderer/resources/renderbuffer.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/vertex.h"

namespace ptgn::impl::gl {

GLRenderer::GLRenderer(Window& window) : gl{ std::make_unique<GLContext>(window) } {
	// TODO: Fix and replace with the object which has a destructor.
	ebo_ = ElementBufferObject{ this,
								gl->buffers.CreateElementBuffer(
									nullptr, kIndexCapacity, sizeof(Index), BufferUsage::DynamicDraw
								) };

	vbo_ = VertexBufferObject{ this, gl->buffers.CreateVertexBuffer(
										 nullptr, kVertexCapacity, sizeof(Vertex),
										 BufferUsage::DynamicDraw
									 ) };

	vao_ =
		VertexArrayObject{ this,
						   gl->vertex_arrays.CreateVertexArray(vbo_, Vertex::GetLayout(), ebo_) };

	white_texture_ = TextureObject{ this, gl->textures.CreateTexture(
											  static_cast<const void*>(&color::White), GL_RGBA,
											  GL_UNSIGNED_INT, { 1, 1 }, GL_RGBA
										  ) };

	// TODO: Use display size instead of window size.
	auto viewport = window.GetSize();

	PTGN_ASSERT(viewport.BothAboveZero(), "Viewport cannot be zero");

	// TODO: Resize screen target when display size event is emitted.
	screen_target_ = CreateRenderTarget(viewport, TextureFormat::RGBA8);
	screen_target_.Bind();
	auto half_viewport{ viewport / 2.0f };
	SetViewProjection(Matrix4::Orthographic(-half_viewport, half_viewport));

	auto max_texture_slots{ gl->GetMaxTextureSlots() };

	std::vector<std::int32_t> samplers(max_texture_slots);
	std::iota(samplers.begin(), samplers.end(), 0);

	auto quad{ gl->shaders.GetProgram("quad") };
	auto _1 = gl->Bind(quad);
	gl->shaders.SetUniform(
		quad, "u_Textures", samplers.data(), static_cast<std::int32_t>(samplers.size())
	);

#ifdef PTGN_PLATFORM_MACOS
	//  Prevents MacOS warning: "UNSUPPORTED (log once): POSSIBLE ISSUE: unit X
	//  GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Float) - using zero
	//  texture because texture unloadable."
	for (std::uint32_t slot{ 0 }; slot < max_texture_slots; slot++) {
		gl->SetActiveTextureSlot(slot);
		auto _3 = gl->Bind(white_texture_);
	}
#endif
	gl->SetActiveTextureSlot(0);
	auto _2 = gl->Bind(white_texture_);

	PTGN_ASSERT(batch_textures_.empty());
	batch_textures_.push_back(white_texture_);
}

GLRenderer::~GLRenderer() noexcept {
	// Needs to have access to GLContext destructor, forward declaration is not enough.
}

RenderTargetObject GLRenderer::CreateRenderTarget(V2_int size, TextureFormat format) {
	const auto& desc = GetTextureFormatDesc(format);

	auto color = gl->textures.CreateTexture(
		nullptr, desc.pixel_format, desc.pixel_type, size, desc.internal_format
	);

	std::optional<RenderbufferId> depth;

	if (desc.has_depth || desc.has_stencil) {
		auto rb_format{ desc.has_stencil ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT };

		depth = gl->renderbuffers.CreateRenderbuffer(size, rb_format);
	}

	auto framebuffer = gl->framebuffers.CreateFramebuffer(
		color, gl::Attachment::Color0, depth,
		desc.has_stencil ? gl::Attachment::DepthStencil : gl::Attachment::Depth
	);

	return RenderTargetObject{ this, RenderTargetData{ framebuffer, color, depth, size, format } };
}

RenderPass GLRenderer::BeginPass(const RenderTargetData& scene_target) {
	RenderPass p;
	p.source_			= scene_target;
	p.ping_				= AcquirePooledTarget(scene_target.size_, scene_target.format_);
	p.has_ping_			= true;
	p.has_written_once_ = false; // latest = source initially
	p.latest_is_ping_	= true;	 // irrelevant until has_written_once==true

	return p;
}

V2_int GLRenderer::GetTextureSize(TextureId texture) const {
	return gl->textures.GetTextureSize(texture);
}

void GLRenderer::FlushBatch() {
	if (batch_indices_.empty()) {
		return; // Nothing to draw
	}

	auto _vao = gl->Bind(vao_);

	// Upload vertex data
	gl->buffers.SetBufferSubData<VertexBufferId>(
		vbo_, BufferTarget::ArrayBuffer, batch_vertices_.data(), 0,
		static_cast<std::uint32_t>(batch_vertices_.size()), sizeof(Vertex)
	);

	// Upload index data
	gl->buffers.SetBufferSubData<ElementBufferId>(
		ebo_, BufferTarget::ElementArrayBuffer, batch_indices_.data(), 0,
		static_cast<std::uint32_t>(batch_indices_.size()), sizeof(Index)
	);

	// Bind all textures
	for (std::uint32_t slot = 0; slot < batch_textures_.size(); ++slot) {
		gl->SetActiveTextureSlot(slot);
		auto _ = gl->Bind(batch_textures_[slot]);
	}

	gl->vertex_arrays.DrawElements(
		vao_, static_cast<std::uint32_t>(batch_indices_.size()), IndexType::UnsignedInt,
		PrimitiveMode::Triangles
	);

	// Clear batch (keep white texture)
	batch_vertices_.clear();
	batch_indices_.clear();
	batch_textures_.resize(1);
	batch_textures_[0] = white_texture_;
}

RenderTargetData GLRenderer::AcquirePooledTarget(V2_int size, TextureFormat format) {
	++pool_tick_;

	auto claim = [&](PooledTarget& e) {
		if (e.target.GetSize() != size) {
			e.target.Resize(size);
		}
		e.in_use		 = true;
		e.last_used_tick = pool_tick_;
		return e.target.resource_;
	};

	// Find a free candidate:
	//  - Prefer exact size+format
	//  - Otherwise pick least-recently-used with same format
	PooledTarget* exact			  = nullptr;
	PooledTarget* lru_same_format = nullptr;

	for (auto& e : rt_pool_) {
		if (e.in_use) {
			continue;
		}
		if (e.target.GetFormat() != format) {
			continue;
		}

		if (e.target.GetSize() == size) {
			exact = &e;
			break; // can't beat an exact match
		}

		if (!lru_same_format || e.last_used_tick < lru_same_format->last_used_tick) {
			lru_same_format = &e;
		}
	}

	if (exact) {
		return claim(*exact);
	}
	if (lru_same_format) {
		return claim(*lru_same_format);
	}

	// No compatible free target available.
	// If we have room in the pool, create one.
	// Pool is at/over the limit and no compatible spare existed:
	PooledTarget entry{ CreateRenderTarget(size, format), pool_tick_, true };
	const auto& rt{ rt_pool_.emplace_back(std::move(entry)) };
	return rt.target.resource_;
}

void GLRenderer::ReleasePooledTarget(RenderTargetData& target) {
	++pool_tick_;

	for (auto& e : rt_pool_) {
		if (e.target.resource_ == target) {
			e.in_use		 = false;
			e.last_used_tick = pool_tick_;
			gl->Destroy(target);
			// TODO: Remove target from rt_pool_.
			return;
		}
	}
}

std::uint32_t GLRenderer::GetTextureSlot(TextureId tex) {
	if (tex == white_texture_.operator Id<TextureTag>()) {
		return 0; // always slot 0
	}

	// Check if texture already exists in batch
	for (std::uint32_t i = 1; i < batch_textures_.size(); ++i) {
		if (batch_textures_[i] == tex) {
			return i;
		}
	}

	// Flush if we would exceed GPU texture slots
	if (batch_textures_.size() >= gl->GetMaxTextureSlots()) {
		FlushBatch();
	}

	// Add texture to batch (but do NOT bind yet)
	batch_textures_.push_back(tex);

	// Its slot is index in the vector
	return static_cast<std::uint32_t>(batch_textures_.size() - 1);
}

template <class State, class Func>
bool UpdateStateIfChanged(GLRenderer& r, const State& cached, const State& desired, Func&& func) {
	if (cached != desired) {
		r.FlushBatch();
		std::invoke(std::forward<Func>(func));
		return true;
	}
	return false;
}

bool GLRenderer::SetViewport(Viewport viewport) {
	return UpdateStateIfChanged(*this, gl->GetBoundState().viewport, viewport, [this, viewport] {
		gl->SetViewport(viewport);
	});
}

bool GLRenderer::SetViewProjection(const Matrix4& view_projection) {
	if (view_projection_ != view_projection) {
		view_projection_ = view_projection;
		FlushBatch();
		return true;
	}
	return false;
}

bool GLRenderer::SetShader(ShaderId shader) {
	return UpdateStateIfChanged(*this, gl->GetBoundState().shader_program, shader, [this, shader] {
		auto _ = gl->Bind(shader);
	});
}

bool GLRenderer::SetBlend(BlendMode mode, bool enabled) {
	BlendState desired{ mode, enabled };

	return UpdateStateIfChanged(*this, gl->GetBoundState().blend, desired, [this, desired] {
		gl->SetBlend(desired);
	});
}

bool GLRenderer::SetFramebuffer(FramebufferId framebuffer) {
	return UpdateStateIfChanged(
		*this, gl->GetBoundState().framebuffer, framebuffer,
		[this, framebuffer] { auto _ = gl->Bind(framebuffer); }
	);
}

bool GLRenderer::SetDepth(const DepthState& depth) {
	return UpdateStateIfChanged(*this, gl->GetBoundState().depth, depth, [this, depth] {
		gl->SetDepth(depth);
	});
}

bool GLRenderer::SetStencil(const StencilState& stencil) {
	return UpdateStateIfChanged(*this, gl->GetBoundState().stencil, stencil, [this, stencil] {
		gl->SetStencil(stencil);
	});
}

bool GLRenderer::SetRaster(const RasterState& raster) {
	return UpdateStateIfChanged(*this, gl->GetBoundState().raster, raster, [this, raster] {
		gl->SetRaster(raster);
	});
}

bool GLRenderer::SetColorMask(const ColorMaskState& color_mask) {
	return UpdateStateIfChanged(
		*this, gl->GetBoundState().color_mask, color_mask,
		[this, color_mask] { gl->SetColorMask(color_mask); }
	);
}

void GLRenderer::DrawQuad(ShaderId shader, const QuadParams& params, const QuadSetup& setup) {
	QuadDesc quad;
	quad.positions = params.positions;
	quad.color	   = params.tint;
	quad.depth	   = params.depth;

	if (params.tex_coords) {
		quad.tex_coords = *params.tex_coords;
	} else if (params.flip_y) {
		quad.tex_coords = { V2_float{ 0.0f, 1.0f }, V2_float{ 1.0f, 1.0f }, V2_float{ 1.0f, 0.0f },
							V2_float{ 0.0f, 0.0f } };
	} else {
		quad.tex_coords = { V2_float{ 0.0f, 0.0f }, V2_float{ 1.0f, 0.0f }, V2_float{ 1.0f, 1.0f },
							V2_float{ 0.0f, 1.0f } };
	}

	// Texture -> user data slot 0 (convention)
	if (params.texture.has_value()) {
		std::uint32_t slot = GetTextureSlot(*params.texture);
		quad.user_data[0]  = static_cast<float>(slot);
	}

	auto updated{ SetShader(shader) };
	// TODO: Dont update view projection every time.
	gl->shaders.SetUniform(shader, "u_ViewProjection", view_projection_);

	setup(shader, quad);

	auto vertices{
		Vertex::GetQuad(quad.positions, quad.color, quad.depth, quad.user_data, quad.tex_coords)
	};

	constexpr std::array<Index, 6> indices{ 0, 1, 2, 2, 3, 0 };

	if (batch_vertices_.size() + vertices.size() >= kVertexCapacity ||
		batch_indices_.size() + indices.size() >= kIndexCapacity) {
		FlushBatch();
	}

	auto start_index = static_cast<std::uint32_t>(batch_vertices_.size());

	batch_vertices_.insert(batch_vertices_.end(), vertices.begin(), vertices.end());

	for (auto idx : indices) {
		batch_indices_.push_back(idx + start_index);
	}
}

void GLRenderer::DrawTriangle(
	ShaderId shader, const std::array<V2_float, 3>& positions, Color tint, float depth
) {
	auto updated{ SetShader(shader) };
	// TODO: Dont update view projection every time.
	gl->shaders.SetUniform(shader, "u_ViewProjection", view_projection_);

	auto vertices{ Vertex::GetTriangle(positions, tint, depth) };

	constexpr std::size_t triangle_indices{ 3 };

	if (batch_vertices_.size() + vertices.size() >= kVertexCapacity ||
		batch_indices_.size() + triangle_indices >= kIndexCapacity) {
		FlushBatch();
	}

	auto start_index = static_cast<std::uint32_t>(batch_vertices_.size());

	batch_vertices_.insert(batch_vertices_.end(), vertices.begin(), vertices.end());
	batch_indices_.insert(batch_indices_.end(), { start_index, start_index + 1, start_index + 2 });
}

void GLRenderer::DrawLine(
	ShaderId shader, const std::array<V2_float, 2>& positions, Color tint, float depth
) {
	auto updated{ SetShader(shader) };
	// TODO: Dont update view projection every time.
	gl->shaders.SetUniform(shader, "u_ViewProjection", view_projection_);

	auto vertices{ Vertex::GetLine(positions, tint, depth) };

	constexpr std::size_t lines_indices{ 2 };

	if (batch_vertices_.size() + vertices.size() >= kVertexCapacity ||
		batch_indices_.size() + lines_indices >= kIndexCapacity) {
		FlushBatch();
	}

	auto start_index = static_cast<std::uint32_t>(batch_vertices_.size());

	batch_vertices_.insert(batch_vertices_.end(), vertices.begin(), vertices.end());
	batch_indices_.insert(batch_indices_.end(), { start_index, start_index + 1 });
}

ShaderId GLRenderer::GetShader(std::string_view name) const {
	return gl->shaders.GetProgram(name);
}

bool GLRenderer::IsTextureAttachedToCurrentFramebuffer(TextureId texture) const {
	auto bound{ gl->GetBoundFramebuffer() };

	if (bound == FramebufferId{ 0 }) {
		return false;
	}

	return gl->framebuffers.GetFramebufferAttachment(bound, Attachment::Color0).id == texture;
}

void GLRenderer::DrawTexture(
	ShaderId shader, TextureId texture, const std::array<V2_float, 4>& positions, Color tint,
	float depth, bool flip_y, const std::optional<std::array<V2_float, 4>>& tex_coords
) {
	PTGN_ASSERT(
		!IsTextureAttachedToCurrentFramebuffer(texture),
		"Cannot draw a texture that is attached to the currently set framebuffer"
	);

	QuadParams p;
	p.positions	 = positions;
	p.depth		 = depth;
	p.tint		 = tint;
	p.texture	 = texture;
	p.flip_y	 = flip_y;
	p.tex_coords = tex_coords;

	DrawQuad(shader, p, [this](auto s, auto& q) {
		gl->shaders.SetUniform(s, "u_Texture", static_cast<std::int32_t>(q.user_data[0]));
	});
}

void GLRenderer::DrawQuad(
	ShaderId shader, const std::array<V2_float, 4>& positions,
	const std::array<float, 4>& user_data, Color tint, float depth
) {
	QuadParams p;
	p.positions = positions;
	p.depth		= depth;
	p.tint		= tint;

	DrawQuad(shader, p, [this, user_data](auto s, auto& q) { q.user_data = user_data; });
}

void GLRenderer::DrawTexture(ShaderId shader, RenderPass& p, const RenderTargetData& scene_target) {
	RenderTargetData input;

	// Input = latest output, or source before first draw
	if (!p.has_written_once_) {
		input = p.source_;
	} else if (p.latest_is_ping_) {
		input = p.ping_;
	} else {
		input = p.pong_;
	}

	PTGN_ASSERT(input.color_.has_value(), "Cannot draw to input texture with no color attachment");

	auto bound_frame_buffer{ gl->GetBoundFramebuffer() };

	// Are we rendering *into this pass*?
	bool writing_to_pass = bound_frame_buffer == p.ping_.framebuffer_ ||
						   (p.has_pong_ && bound_frame_buffer == p.pong_.framebuffer_);

	bool input_is_offscreen = input.framebuffer_ != scene_target.framebuffer_;

	bool output_is_offscreen = bound_frame_buffer != scene_target.framebuffer_;

	bool flip_y = input_is_offscreen && !output_is_offscreen;

	// Only ping-pong if we're writing into the pass
	if (writing_to_pass) {
		RenderTargetData write;

		if (!p.has_written_once_) {
			write = p.ping_;
		} else {
			if (!p.has_pong_ && p.latest_is_ping_) {
				p.pong_		= AcquirePooledTarget(p.source_.size_, p.source_.format_);
				p.has_pong_ = true;
			}
			write = p.latest_is_ping_ ? p.pong_ : p.ping_;
		}

		write.Bind(*this);

		DrawTexture(
			shader, *input.color_,
			GetCenteredQuadPoints(gl->textures.GetTextureSize(*input.color_)), color::White, 0.0f,
			flip_y, {}
		);

		// Update pass state
		p.has_written_once_ = true;
		p.latest_is_ping_	= (write.framebuffer_ == p.ping_.framebuffer_);
	} else {
		// Read-only draw: no mutation, no flip
		DrawTexture(
			shader, *input.color_,
			GetCenteredQuadPoints(gl->textures.GetTextureSize(*input.color_)), color::White, 0.0f,
			flip_y, {}
		);
	}
}

void GLRenderer::BeginFrame(V2_int window_size) {
	PTGN_ASSERT(batch_vertices_.empty());
	PTGN_ASSERT(batch_indices_.empty());

	auto _1 = gl->Bind(FramebufferId{ 0 });
	gl->SetClearColor(color::Transparent);
	SetViewport({ {}, window_size });
	gl->framebuffers.Clear();

	screen_target_.Bind();
	SetViewport({ {}, screen_target_.GetSize() });
	gl->framebuffers.ClearToColor(screen_target_.resource_.framebuffer_, color::Transparent);
}

void GLRenderer::EndFrame(Viewport display_viewport) {
	PTGN_ASSERT(display_viewport.size.BothAboveZero());

	SetFramebuffer({});

	auto half_viewport{ display_viewport.size * 0.5f };
	SetViewport(display_viewport);
	SetViewProjection(Matrix4::Orthographic(-half_viewport, half_viewport));
	SetBlend(BlendMode::ReplaceRGBA);

	PTGN_ASSERT(
		screen_target_.resource_.color_.has_value(),
		"Cannot draw to screen target with no color attachment"
	);

	DrawTexture(
		GetShader("quad"), *screen_target_.resource_.color_,
		GetCenteredQuadPoints(display_viewport.size), color::White, 0.0f, true, {}
	);

	FlushBatch();
}

TextureId GLRenderer::GetWhiteTexture() const {
	return white_texture_;
}

void GLRenderer::ResizeScreenTarget(V2_int size) {
	return screen_target_.Resize(size);
}

void GLRenderer::BindScreenTarget() {
	screen_target_.Bind();
}

} // namespace ptgn::impl::gl