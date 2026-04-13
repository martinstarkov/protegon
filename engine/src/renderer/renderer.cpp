
#include "renderer/renderer.h"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "platform/window.h"
#include "primitives/event.h"
#include "renderer/backend/gl/gl_buffer.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_shader.h"
#include "renderer/backend/gl/gl_state.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/backend/gl/gl_vertex_array.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/render_pass.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/buffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "renderer/resources/vertex_array.h"
#include "renderer/vertex/vertex.h"
#include "runtime/event/event_handler.h"

namespace ptgn {

Renderer::Renderer(Window& window, EventHandler& events) :
	window_{ window }, events_{ events }, gl_{ std::make_unique<impl::gl::GLContext>(window) } {
	ebo_ = impl::ElementBufferObject{ this, gl_->buffers.CreateElementBuffer(
												nullptr, impl::kIndexCapacity, sizeof(impl::Index),
												impl::gl::BufferUsage::DynamicDraw
											) };

	vbo_ = impl::VertexBufferObject{ this, gl_->buffers.CreateVertexBuffer(
											   nullptr, impl::kVertexCapacity, sizeof(impl::Vertex),
											   impl::gl::BufferUsage::DynamicDraw
										   ) };

	vao_ = impl::VertexArrayObject{
		this, gl_->vertex_arrays.CreateVertexArray(vbo_, impl::Vertex::GetLayout(), ebo_)
	};

	// Important to use unsigned byte for the white texture as color::White is stored in
	// std::uint8_t.
	constexpr impl::gl::PixelDataType pixel_type{ impl::gl::PixelDataType::UnsignedByte };

	white_texture_ = impl::TextureObject{ this, gl_->textures.CreateTexture(
													static_cast<const void*>(&color::White),
													impl::gl::PixelDataFormat::RGBA, pixel_type,
													V2_int{ 1, 1 }, TextureFormat::RGBA8
												) };

	auto viewport{ window.GetSize() };

	PTGN_ASSERT(viewport.BothAboveZero(), "Viewport cannot be zero");

	screen_target_ = CreateRenderTarget(viewport, TextureFormat::RGBA8);
	BindScreenTarget();
	V2_float half_viewport{ viewport / 2.0f };
	auto view_projection{ Matrix4::Orthographic(-half_viewport, half_viewport) };
	SetViewProjection(view_projection);

	auto max_texture_slots{ gl_->GetMaxTextureSlots() };

	std::vector<std::int32_t> samplers(max_texture_slots);
	std::iota(samplers.begin(), samplers.end(), 0);

	auto quad{ gl_->shaders.GetProgram("quad") };
	auto _1 = gl_->Bind(quad, false);
	SetUniform(quad, "u_Textures", samplers);

#ifdef PTGN_PLATFORM_MACOS
	//  Prevents MacOS warning: "UNSUPPORTED (log once): POSSIBLE ISSUE: unit X
	//  GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Float) - using zero
	//  texture because texture unloadable."
	for (std::uint32_t slot{ 0 }; slot < max_texture_slots; slot++) {
		gl_->SetActiveTextureSlot(slot);
		auto _3 = gl_->Bind(white_texture_, false);
	}
#endif
	gl_->SetActiveTextureSlot(0);
	auto _2 = gl_->Bind(white_texture_, false);

	PTGN_ASSERT(batch_textures_.empty());
	batch_textures_.push_back(white_texture_);
}

Renderer::~Renderer() noexcept {
	// Destructor access to impl::gl::GLContext is needed.
}

impl::RenderTargetObject Renderer::CreateRenderTarget(V2_int size, TextureFormat format) {
	auto color = gl_->textures.CreateTexture(size, format);

	std::optional<impl::RenderbufferId> depth;

	if (!IsColorFormat(format)) {
		depth = gl_->renderbuffers.CreateRenderbuffer(size, format);
	}

	auto framebuffer = gl_->framebuffers.CreateFramebuffer(
		color, impl::gl::Attachment::Color0, depth,
		IsDepthOnlyFormat(format) ? impl::gl::Attachment::Depth : impl::gl::Attachment::DepthStencil
	);

	return impl::RenderTargetObject{ this, impl::RenderTargetId{ framebuffer } };
}

impl::RenderPass Renderer::BeginPass(impl::RenderTargetId scene_render_target) {
	impl::RenderPass p;
	p.source_			= scene_render_target;
	p.ping_				= AcquirePooledTargetCopy(scene_render_target);
	p.has_ping_			= true;
	p.has_written_once_ = false; // latest = source initially
	p.latest_is_ping_	= true;	 // irrelevant until has_written_once==true

	return p;
}

impl::RenderTargetId Renderer::AcquirePooledTargetCopy(impl::RenderTargetId render_target) {
	auto size{ GetRenderTargetSize(render_target) };
	auto format{ GetRenderTargetTextureFormat(render_target) };

	return AcquirePooledTarget(size, format);
}

impl::RenderTargetId Renderer::AcquirePooledTarget(V2_int size, TextureFormat format) {
	++pool_tick_;

	auto claim = [&](impl::PooledTarget& e) {
		if (e.target.GetSize() != size) {
			ResizeRenderTarget(e.target.resource_, size);
		}
		e.in_use		 = true;
		e.last_used_tick = pool_tick_;
		return e.target.resource_;
	};

	// Find a free candidate:
	//  - Prefer exact size+format
	//  - Otherwise pick least-recently-used with same format
	impl::PooledTarget* exact			= nullptr;
	impl::PooledTarget* lru_same_format = nullptr;

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
	impl::PooledTarget entry{ CreateRenderTarget(size, format), pool_tick_, true };
	const auto& rt{ rt_pool_.emplace_back(std::move(entry)) };

	return rt.target.resource_;
}

void Renderer::ReleasePooledTarget(impl::RenderTargetId render_target) {
	++pool_tick_;

	std::erase_if(rt_pool_, [render_target](auto& e) {
		if (e.target.resource_ == render_target) {
			return true; // remove from pool
		}
		return false;
	});
}

void Renderer::DrawTexture(
	impl::ShaderId shader, impl::RenderPass& p, impl::RenderTargetId scene_render_target,
	const std::function<void()>& shader_setup
) {
	impl::RenderTargetId input;

	// Input = latest output, or source before first draw
	if (!p.has_written_once_) {
		input = p.source_;
	} else if (p.latest_is_ping_) {
		input = p.ping_;
	} else {
		input = p.pong_;
	}

	auto bound_frame_buffer{ gl_->GetBoundFramebuffer() };

	// Are we rendering *into this pass*?
	bool writing_to_pass =
		bound_frame_buffer == p.ping_ || (p.has_pong_ && bound_frame_buffer == p.pong_);

	bool input_is_offscreen = input != scene_render_target;

	bool output_is_offscreen = bound_frame_buffer != scene_render_target;

	bool flip_y = input_is_offscreen && !output_is_offscreen;

	auto texture_size{ GetRenderTargetSize(input) };
	auto points{ impl::GetCenteredQuadPoints(texture_size) };
	auto tex_coords{ impl::GetDefaultTextureCoordinates(flip_y) };

	// Only ping-pong if we're writing into the pass
	if (writing_to_pass) {
		impl::RenderTargetId write;

		if (!p.has_written_once_) {
			write = p.ping_;
		} else {
			if (!p.has_pong_ && p.latest_is_ping_) {
				p.pong_		= AcquirePooledTargetCopy(p.source_);
				p.has_pong_ = true;
			}
			write = p.latest_is_ping_ ? p.pong_ : p.ping_;
		}

		BindRenderTarget(write);

		auto texture{ GetRenderTargetTexture(input) };

		DrawTexture(shader, texture, points, color::White, 0.0f, tex_coords, shader_setup);

		// Update pass state
		p.has_written_once_ = true;
		p.latest_is_ping_	= write == p.ping_;
	} else {
		auto texture{ GetRenderTargetTexture(input) };
		// Read-only draw: no mutation, no flip
		DrawTexture(shader, texture, points, color::White, 0.0f, tex_coords, shader_setup);
	}
}

void Renderer::Destroy(impl::VertexBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::ElementBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::UniformBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::ShaderId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::TextureId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::RenderbufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::FramebufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::VertexArrayId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(impl::RenderTargetId id) {
	gl_->Destroy(id);
};

V2_int Renderer::GetTextureSize(impl::TextureId texture) const {
	return gl_->textures.GetTextureSize(texture);
}

TextureFormat Renderer::GetTextureFormat(impl::TextureId texture) const {
	return gl_->textures.GetTextureFormat(texture);
}

void Renderer::ResizeRenderTarget(impl::RenderTargetId render_target, V2_int new_size) {
	gl_->framebuffers.ResizeFramebuffer(impl::FramebufferId{ render_target }, new_size);
}

impl::TextureId Renderer::GetRenderTargetTexture(impl::RenderTargetId render_target) const {
	const auto& color_attachment{ gl_->framebuffers.GetFramebufferAttachment(
		impl::FramebufferId{ render_target }, impl::gl::Attachment::Color0
	) };
	PTGN_ASSERT(
		color_attachment.id,
		"Render target must have a valid color attachment for its texture to be retrieved"
	);
	return impl::TextureId{ color_attachment.id };
}

V2_int Renderer::GetRenderTargetSize(impl::RenderTargetId render_target) const {
	auto id{ GetRenderTargetTexture(render_target) };

	auto size{ gl_->textures.GetTextureSize(id) };

	return size;
}

TextureFormat Renderer::GetRenderTargetTextureFormat(impl::RenderTargetId render_target) const {
	auto id{ GetRenderTargetTexture(render_target) };

	auto texture_format{ gl_->textures.GetTextureFormat(id) };

	return texture_format;
}

void Renderer::ClearRenderTarget(impl::RenderTargetId render_target, Color color, bool set_viewport)
	const {
	auto bind_guard = gl_->Bind(impl::FramebufferId{ render_target }, true);

	std::optional<Viewport> viewport;
	if (set_viewport) {
		viewport = gl_->GetViewport();

		auto render_target_size{ GetRenderTargetSize(render_target) };

		gl_->SetViewport({ {}, render_target_size });
	}

	gl_->framebuffers.ClearToColor(impl::FramebufferId{ render_target }, color);

	if (set_viewport && viewport.has_value()) {
		gl_->SetViewport(*viewport);
	}
}

void Renderer::BindRenderTarget(impl::RenderTargetId render_target) {
	SetFramebuffer(impl::FramebufferId{ render_target });
}

void Renderer::BindRenderPass(impl::RenderPass& render_pass) {
	// Bind the next write target (opposite of latest output; ping for first write)
	impl::RenderTargetId write;

	if (!render_pass.has_written_once_) {
		write = render_pass.ping_;
	} else {
		if (!render_pass.has_pong_ && render_pass.latest_is_ping_) {
			render_pass.pong_	  = AcquirePooledTargetCopy(render_pass.source_);
			render_pass.has_pong_ = true;
		}
		write = render_pass.latest_is_ping_ ? render_pass.pong_ : render_pass.ping_;
	}

	BindRenderTarget(write);
}

void Renderer::FlushBatch() {
	if (batch_indices_.empty()) {
		return; // Nothing to draw
	}

	auto _vao = gl_->Bind(vao_, false);

	// Upload vertex data
	gl_->buffers.SetBufferSubData<impl::VertexBufferId>(
		vbo_, impl::gl::BufferTarget::ArrayBuffer, batch_vertices_.data(), 0,
		static_cast<std::uint32_t>(batch_vertices_.size()), sizeof(impl::Vertex)
	);

	// Upload index data
	gl_->buffers.SetBufferSubData<impl::ElementBufferId>(
		ebo_, impl::gl::BufferTarget::ElementArrayBuffer, batch_indices_.data(), 0,
		static_cast<std::uint32_t>(batch_indices_.size()), sizeof(impl::Index)
	);

	// Bind all textures
	for (std::uint32_t slot = 0; slot < batch_textures_.size(); ++slot) {
		gl_->SetActiveTextureSlot(slot);
		auto _ = gl_->Bind(batch_textures_[slot], false);
	}

	gl_->vertex_arrays.DrawElements(
		vao_, static_cast<std::uint32_t>(batch_indices_.size()), impl::gl::IndexType::UnsignedInt,
		impl::gl::PrimitiveMode::Triangles
	);

	// Clear batch (keep white texture)
	batch_vertices_.clear();
	batch_indices_.clear();
	batch_textures_.resize(1);
	batch_textures_[0] = white_texture_;
}

std::pair<std::uint32_t, bool> Renderer::GetTextureSlot(impl::TextureId tex) {
	if (tex == white_texture_.operator impl::TextureId()) {
		return { 0, false }; // always slot 0
	}

	// Check if texture already exists in batch
	for (std::uint32_t i = 1; i < batch_textures_.size(); ++i) {
		if (batch_textures_[i] == tex) {
			return { i, false };
		}
	}

	// Flush if we would exceed GPU texture slots
	if (batch_textures_.size() >= gl_->GetMaxTextureSlots()) {
		FlushBatch();
	}

	// Its slot is index in the vector
	return { static_cast<std::uint32_t>(batch_textures_.size()), true };
}

namespace impl {

template <typename State, typename F>
	requires std::same_as<std::invoke_result_t<F&>, void>
void UpdateStateIfChanged(Renderer& r, const State& cached, const State& desired, F&& func) {
	if (cached != desired) {
		r.FlushBatch();
		std::invoke(std::forward<F>(func));
	}
}

} // namespace impl

void Renderer::SetViewport(Viewport viewport) {
	impl::UpdateStateIfChanged(*this, gl_->GetBoundState().viewport, viewport, [this, viewport] {
		gl_->SetViewport(viewport);
	});
}

void Renderer::SetViewProjection(const Matrix4& view_projection) {
	if (view_projection_ != view_projection) {
		FlushBatch();
		view_projection_ = view_projection;
	}
	// TODO: Find a better way to do this. This is needed to ensure that the shader's uniform is
	// updated even if the shader itself doesn't change.
	if (auto shader{ gl_->GetBoundShader() }; shader) {
		gl_->shaders.SetUniform(shader, "u_ViewProjection", view_projection_);
	}
}

void Renderer::SetShader(impl::ShaderId shader) {
	if (gl_->GetBoundState().shader_program != shader) {
		FlushBatch();
		auto _ = gl_->Bind(shader, false);
		gl_->shaders.SetUniform(shader, "u_ViewProjection", view_projection_);
	}
}

void Renderer::SetBlend(BlendMode mode, bool enabled) {
	BlendState desired{ mode, enabled };

	impl::UpdateStateIfChanged(*this, gl_->GetBoundState().blend, desired, [this, desired] {
		gl_->SetBlend(desired);
	});
}

void Renderer::SetFramebuffer(impl::FramebufferId framebuffer) {
	if (gl_->GetBoundState().framebuffer != framebuffer) {
		FlushBatch();
		auto _ = gl_->Bind(framebuffer, false);
	}
}

void Renderer::SetDepth(const DepthState& depth) {
	impl::UpdateStateIfChanged(*this, gl_->GetBoundState().depth, depth, [this, depth] {
		gl_->SetDepth(depth);
	});
}

void Renderer::SetStencil(const StencilState& stencil) {
	impl::UpdateStateIfChanged(*this, gl_->GetBoundState().stencil, stencil, [this, stencil] {
		gl_->SetStencil(stencil);
	});
}

void Renderer::SetRaster(const RasterState& raster) {
	impl::UpdateStateIfChanged(*this, gl_->GetBoundState().raster, raster, [this, raster] {
		gl_->SetRaster(raster);
	});
}

void Renderer::SetScissor(const ScissorState& scissor) {
	impl::UpdateStateIfChanged(*this, gl_->GetBoundState().scissor, scissor, [this, scissor] {
		gl_->SetScissor(scissor);
	});
}

void Renderer::SetColorMask(const ColorMaskState& color_mask) {
	impl::UpdateStateIfChanged(
		*this, gl_->GetBoundState().color_mask, color_mask,
		[this, color_mask] { gl_->SetColorMask(color_mask); }
	);
}

void Renderer::FlushIfExceedsCapacity(std::size_t vertices, std::size_t indices) {
	if (batch_vertices_.size() + vertices > impl::kVertexCapacity ||
		batch_indices_.size() + indices > impl::kIndexCapacity) {
		FlushBatch();
	}
}

void Renderer::DrawQuad(
	impl::ShaderId shader, const impl::QuadParams& params,
	const std::function<bool(impl::ShaderId, impl::QuadDesc&)>& setup
) {
	impl::QuadDesc quad;
	quad.quad = params.quad;

	bool push_texture{ false };

	// Texture -> user data slot 0 (convention)
	if (params.texture.has_value()) {
		auto [slot, push] = GetTextureSlot(*params.texture);
		push_texture	  = push;
		quad.user_data[0] = static_cast<float>(slot);
	}

	SetShader(shader);

	bool flush_after{ setup(shader, quad) };

	auto vertices{ impl::Vertex::GetQuad(
		quad.quad.positions, quad.quad.color, quad.quad.depth, quad.user_data, quad.quad.tex_coords
	) };

	constexpr std::array<impl::Index, 6> indices{ 0, 1, 2, 2, 3, 0 };

	FlushIfExceedsCapacity(vertices.size(), indices.size());

	auto start_index = static_cast<std::uint32_t>(batch_vertices_.size());

	batch_vertices_.insert(batch_vertices_.end(), vertices.begin(), vertices.end());

	for (auto idx : indices) {
		batch_indices_.push_back(idx + start_index);
	}

	if (push_texture) {
		PTGN_ASSERT(params.texture.has_value(), "Texture must have a value for it to be pushed");
		batch_textures_.push_back(*params.texture);
	}

	if (flush_after) {
		FlushBatch();
	}
}

void Renderer::DrawTriangle(
	impl::ShaderId shader, const std::array<V2_float, 3>& positions, Color tint, float depth
) {
	SetShader(shader);

	auto vertices{ impl::Vertex::GetTriangle(positions, tint, depth) };

	constexpr std::size_t triangle_indices{ 3 };

	FlushIfExceedsCapacity(vertices.size(), triangle_indices);

	auto start_index = static_cast<std::uint32_t>(batch_vertices_.size());

	batch_vertices_.insert(batch_vertices_.end(), vertices.begin(), vertices.end());
	batch_indices_.insert(batch_indices_.end(), { start_index, start_index + 1, start_index + 2 });
}

impl::ShaderId Renderer::GetShader(std::string_view name) const {
	return gl_->shaders.GetProgram(name);
}

bool Renderer::IsTextureAttachedToCurrentFramebuffer(impl::TextureId texture) const {
	auto bound{ gl_->GetBoundFramebuffer() };

	if (bound == impl::FramebufferId{ 0 }) {
		return false;
	}

	return gl_->framebuffers.GetFramebufferAttachment(bound, impl::gl::Attachment::Color0).id ==
		   texture;
}

void Renderer::DrawTexture(
	impl::ShaderId shader, impl::TextureId texture, const std::array<V2_float, 4>& positions,
	Color tint, float depth, const std::array<V2_float, 4>& tex_coords,
	const std::function<void()>& shader_setup
) {
	PTGN_ASSERT(
		!IsTextureAttachedToCurrentFramebuffer(texture),
		"Cannot draw a texture that is attached to the currently set framebuffer"
	);

	impl::QuadParams p;
	p.quad.positions  = positions;
	p.quad.depth	  = depth;
	p.quad.color	  = tint;
	p.quad.tex_coords = tex_coords;
	p.texture		  = texture;

	auto setup = [this, shader_setup](auto s, auto& q) {
		// Only quad drawn textures are batchable.
		bool batchable{ s == gl_->shaders.GetProgram("quad") };

		if (!batchable) {
			gl_->shaders.SetUniform(s, "u_Texture", static_cast<std::int32_t>(q.user_data[0]));
		} else {
			PTGN_ASSERT(!shader_setup, "Quad shader should have no shader setup");
		}

		if (!batchable && shader_setup) {
			shader_setup();
		}

		return !batchable;
	};

	DrawQuad(shader, p, setup);
}

void Renderer::DrawQuad(
	impl::ShaderId shader, const std::array<V2_float, 4>& positions,
	const std::array<float, 4>& user_data, Color tint, float depth,
	const std::function<void()>& shader_setup
) {
	impl::QuadParams p;
	p.quad.positions  = positions;
	p.quad.depth	  = depth;
	p.quad.color	  = tint;
	p.quad.tex_coords = impl::GetDefaultTextureCoordinates<false>();

	DrawQuad(shader, p, [this, user_data, shader_setup](auto, auto& q) {
		q.user_data = user_data;
		if (shader_setup) {
			shader_setup();
			return true;
		}
		return false;
	});
}

impl::TextureId Renderer::GetWhiteTexture() const {
	return white_texture_;
}

void Renderer::OnWindowResize(V2_int size) {
	if (!game_size_.has_value()) {
		// PTGN_LOG("Emitting game resized: ", size);
		events_.Push<impl::event::InternalGameResized>(size);
		events_.Push<event::GameResized>(size);
	}

	UpdateDisplayViewport(size);
}

void Renderer::SetScalingMode(ScalingMode scaling_mode) {
	if (scaling_mode_ == scaling_mode) {
		return;
	}

	scaling_mode_ = scaling_mode;

	UpdateDisplayViewport(window_.GetSize());
}

void Renderer::SetGameSize(std::optional<V2_int> game_size, ScalingMode scaling_mode) {
	if (game_size_ == game_size && scaling_mode_ == scaling_mode) {
		return;
	}

	PTGN_ASSERT(
		!game_size.has_value() || game_size.has_value() && game_size->BothAboveZero(),
		"Game size cannot be negative or zero"
	);

	game_size_	  = game_size;
	scaling_mode_ = scaling_mode;

	auto size{ GetGameSize() };

	// PTGN_LOG("Emitting game resized: ", size);
	events_.Push<impl::event::InternalGameResized>(size);
	events_.Push<event::GameResized>(size);

	UpdateDisplayViewport(window_.GetSize());
}

V2_int Renderer::GetDisplaySize() const {
	return display_viewport_.size;
}

V2_float Renderer::GetScale() const {
	auto display_size{ GetDisplaySize() };
	auto game_size{ GetGameSize() };

	PTGN_ASSERT(display_size.BothAboveZero());
	PTGN_ASSERT(game_size.BothAboveZero());

	return V2_float{ display_size } / game_size;
}

V2_int Renderer::GetGameSize() const {
	if (game_size_) {
		return *game_size_;
	}
	return window_.GetSize();
}

ScalingMode Renderer::GetScalingMode() const {
	return scaling_mode_;
}

void Renderer::SetBackgroundColor(Color background_color) {
	background_color_.value = background_color;
}

Color Renderer::GetBackgroundColor() const {
	return background_color_.value;
}

Viewport Renderer::GetDisplayViewport() const {
	return display_viewport_;
}

void Renderer::UpdateDisplayViewport(V2_int window_size, bool emit_events) {
	PTGN_ASSERT(window_size == window_.GetSize());

	auto game_size{ game_size_.value_or(window_size) };

	PTGN_ASSERT(window_size.BothAboveZero());
	PTGN_ASSERT(game_size.BothAboveZero());

	Viewport viewport{ .position = { 0, 0 }, .size = window_size };

	auto compute_aspect_fit = [&viewport, game_size, window_size](bool letterbox_mode) {
		float window_aspect{ static_cast<float>(window_size.x) / window_size.y };
		float game_aspect{ static_cast<float>(game_size.x) / game_size.y };

		// In letterbox mode we need require window_aspect > game_aspect to fit height, and in
		// overscan we require window_aspect > game_aspect to fit height.
		bool fit_height{ (window_aspect > game_aspect) == letterbox_mode };

		if (fit_height) {
			viewport.size.y = window_size.y;
			viewport.size.x =
				static_cast<int>(static_cast<float>(window_size.y) * game_aspect + 0.5f);
			viewport.position.x = (window_size.x - viewport.size.x) / 2; // left edge.
			viewport.position.y = 0;
		} else {
			// Fit width.
			viewport.size.x = window_size.x;
			viewport.size.y =
				static_cast<int>(static_cast<float>(window_size.x) / game_aspect + 0.5f);
			viewport.position.x = 0;
			viewport.position.y = (window_size.y - viewport.size.y) / 2; // top edge.
		}
	};

	switch (scaling_mode_) {
		case ScalingMode::Letterbox: compute_aspect_fit(true); break;
		case ScalingMode::Overscan:	 compute_aspect_fit(false); break;

		case ScalingMode::Stretch:
			PTGN_ASSERT(viewport.position == V2_int{});
			PTGN_ASSERT(viewport.size == window_.GetSize());
			// Viewport is full window (default).
			break;

		case ScalingMode::IntegerScale: {
			V2_int ratio{ window_size / game_size };
			// Find which dimension limits the scaling factor.
			int scale{ std::max(1, std::min(ratio.x, ratio.y)) };
			viewport.size	  = game_size * scale;				   // scale up.
			viewport.position = (window_size - viewport.size) / 2; // center of window.
			break;
		}

		case ScalingMode::Disabled:
			viewport.size	  = game_size;						   // no change.
			viewport.position = (window_size - viewport.size) / 2; // center of window.
			break;

		default: PTGN_ERROR("Unsupported resolution mode");
	}

	bool resized{ viewport.size != display_viewport_.size };
	bool moved{ viewport.position != display_viewport_.position };

	if (resized || moved) {
		PTGN_ASSERT(viewport.size.BothAboveZero());

		display_viewport_ = viewport;

		if (resized) {
			ResizeScreenTarget(display_viewport_.size);

			if (emit_events) {
				// PTGN_LOG("Emitting display resized: ", display_viewport_.size);
				events_.Push<impl::event::InternalDisplayResized>(display_viewport_.size);
			}
		}

		if (emit_events) {
			// PTGN_LOG("Emitting viewport changed: ", display_viewport_);
			events_.Push<impl::event::InternalDisplayViewportChanged>(display_viewport_);
		}
	}
}

void Renderer::ResizeScreenTarget(V2_int size) {
	ResizeRenderTarget(screen_target_.resource_, size);
}

void Renderer::BindScreenTarget() {
	BindRenderTarget(screen_target_);
}

void Renderer::InvalidateState() {
	gl_->
}

void Renderer::BeginFrame() {
	InvalidateState();

	PTGN_ASSERT(batch_vertices_.empty());
	PTGN_ASSERT(batch_indices_.empty());

	V2_int window_size{ window_.GetSize() };
	Color window_background_color{ window_.GetBackgroundColor() };

	auto _1 = gl_->Bind(impl::FramebufferId{ 0 }, false);
	gl_->SetClearColor(window_background_color);
	SetViewport({ {}, window_size });
	gl_->framebuffers.Clear();

	BindScreenTarget();
	SetViewport({ {}, screen_target_.GetSize() });
	gl_->framebuffers.ClearToColor(
		impl::FramebufferId{ screen_target_.resource_ }, background_color_.value
	);
}

void Renderer::EndFrame() {
	PTGN_ASSERT(display_viewport_.size.BothAboveZero());

	SetFramebuffer({});

	V2_float half_viewport{ display_viewport_.size * 0.5f };
	SetViewport(display_viewport_);
	auto view_projection{ Matrix4::Orthographic(-half_viewport, half_viewport) };
	SetViewProjection(view_projection);
	SetBlend(BlendMode::ReplaceRGBA, true);

	PTGN_ASSERT(
		GetRenderTargetSize(screen_target_.resource_) == display_viewport_.size,
		"Screen target texture size must match display viewport size"
	);
	auto quad_shader{ GetShader("quad") };
	auto points{ impl::GetCenteredQuadPoints(display_viewport_.size) };
	auto tex_coords{ impl::GetDefaultTextureCoordinates<true>() };

	auto screen_texture{ GetRenderTargetTexture(screen_target_.resource_) };

	DrawTexture(quad_shader, screen_texture, points, color::White, 0.0f, tex_coords, {});

	FlushBatch();
}

impl::ShaderObject Renderer::CreateShader(
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
) {
	return impl::ShaderObject{ this, gl_->shaders.CreateProgram(source, shader_name) };
}

impl::TextureObject Renderer::CreateTexture(
	const std::uint8_t* pixel_data, V2_int size, TextureFormat format
) {
	auto [pixel_format, pixel_type] = impl::gl::GetPixelDataFormat(format);
	PTGN_ASSERT(
		pixel_type == impl::gl::PixelDataType::UnsignedByte,
		"Texture format must have a type of bytes"
	);
	return impl::TextureObject{
		this, gl_->textures.CreateTexture(pixel_data, pixel_format, pixel_type, size, format)
	};
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, const Matrix4& v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, V2_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, V3_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, V4_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(
	impl::ShaderId shader, const char* uniform_name, const std::vector<float>& v
) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, V2_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, V3_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, V4_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(
	impl::ShaderId shader, const char* uniform_name, const std::vector<int>& v
) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(impl::ShaderId shader, const char* uniform_name, bool v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

} // namespace ptgn