
#include "renderer/renderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <string_view>
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
#include "core/util/concepts.h"
#include "core/util/hash.h"
#include "platform/window.h"
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
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/primitive_mode.h"
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

namespace ptgn::impl {

Renderer::Renderer(Window& window, EventSink&& event_sink) :
	window_{ window },
	event_sink_{ std::move(event_sink) },
	gl_{ std::make_unique<gl::GLContext>() } {
	AddPipeline<TextureVertex>(
		"texture", kVertexCapacity, kIndexCapacity, PrimitiveMode::Triangles
	);
	AddPipeline<ShapeVertex>("shape", kVertexCapacity, kIndexCapacity, PrimitiveMode::Triangles);
	AddPipeline<ColorVertex>("color", kVertexCapacity, kIndexCapacity, PrimitiveMode::Triangles);
	SetPipeline("texture");

	game_size_ = GetFullViewportSize();

	auto display{ RecalculateDisplayViewport() };

	display_viewport_		= display.viewport;
	display_viewport_dirty_ = false;

	auto display_size{ GetDisplaySize() };

	PTGN_ASSERT(display_size.BothAboveZero(), "Display size cannot be zero");

	screen_target_ = CreateRenderTarget(display_size, TextureFormat::RGBA8);
	BindScreenTarget();
	V2_float half_viewport{ display_size / 2.0f };
	auto view_projection{ Matrix4::Orthographic(-half_viewport, half_viewport) };
	SetViewProjection(view_projection);

	auto max_texture_slots{ gl_->GetMaxTextureSlots() };

	std::vector<std::int32_t> samplers(max_texture_slots);
	std::iota(samplers.begin(), samplers.end(), 0);

	auto quad{ gl_->shaders.GetProgram("texture") };
	auto _1 = gl_->Bind(quad, false);
	SetUniform(quad, "u_Textures", samplers);

#ifdef PTGN_PLATFORM_MACOS
	//  Prevents MacOS warning: "UNSUPPORTED (log once): POSSIBLE ISSUE: unit X
	//  GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Float) - using zero
	//  texture because texture unloadable."
	for (std::uint32_t slot{ 0 }; slot < max_texture_slots; slot++) {
		gl_->SetActiveTextureSlot(slot);
		auto _3 = gl_->Bind(TextureId{ 0 }, false);
	}
#endif
}

Renderer::~Renderer() noexcept {
	// Guarantees that a vertex array object is bound before destroying any buffers.
	auto _{ gl_->Bind(VertexArrayId{ 0 }, false) };
}

RenderTargetObject Renderer::CreateRenderTarget(V2_int size, TextureFormat format) {
	auto color = gl_->textures.CreateTexture(size, format);

	std::optional<RenderbufferId> depth;

	if (!IsColorFormat(format)) {
		depth = gl_->renderbuffers.CreateRenderbuffer(size, format);
	}

	using enum gl::Attachment;

	auto framebuffer = gl_->framebuffers.CreateFramebuffer(
		color, Color0, depth, IsDepthOnlyFormat(format) ? Depth : DepthStencil
	);

	return RenderTargetObject{ this, RenderTargetId{ framebuffer } };
}

RenderPass Renderer::BeginPass(RenderTargetId scene_render_target) {
	RenderPass p;
	p.source_			= scene_render_target;
	p.ping_				= AcquirePooledTargetCopy(scene_render_target);
	p.has_ping_			= true;
	p.has_written_once_ = false; // latest = source initially
	p.latest_is_ping_	= true;	 // irrelevant until has_written_once==true

	return p;
}

RenderTargetId Renderer::AcquirePooledTargetCopy(RenderTargetId render_target) {
	auto size{ GetRenderTargetSize(render_target) };
	auto format{ GetRenderTargetTextureFormat(render_target) };

	return AcquirePooledTarget(size, format);
}

RenderTargetId Renderer::AcquirePooledTarget(V2_int size, TextureFormat format) {
	++pool_tick_;

	auto claim = [&](PooledTarget& e) {
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

void Renderer::ReleasePooledTarget(RenderTargetId render_target) {
	++pool_tick_;

	std::erase_if(rt_pool_, [render_target](auto& e) {
		if (e.target.resource_ == render_target) {
			return true; // remove from pool
		}
		return false;
	});
}

void Renderer::DrawRenderPass(
	ShaderId shader, RenderPass& p, RenderTargetId scene_render_target,
	const std::function<void()>& shader_setup
) {
	RenderTargetId input;

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
	auto points{ GetCenteredQuadPoints(texture_size) };
	auto tex_coords{ GetDefaultTextureCoordinates(flip_y) };

	TextureId texture{ GetRenderTargetTexture(input) };

	// Only ping-pong if we're writing into the pass
	if (writing_to_pass) {
		RenderTargetId write;

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

		// Update pass state
		p.has_written_once_ = true;
		p.latest_is_ping_	= write == p.ping_;
	} else {
		// Read-only draw: no mutation, no flip
	}

	DrawTexture(shader, texture, points, 0.0f, color::White, tex_coords, shader_setup, -1);
}

TextureId Renderer::GetRenderTargetTexture(RenderTargetId render_target) const {
	const auto& color_attachment{ gl_->framebuffers.GetFramebufferAttachment(
		FramebufferId{ render_target }, gl::Attachment::Color0
	) };
	PTGN_ASSERT(
		color_attachment.id,
		"Render target must have a valid color attachment for its texture to be retrieved"
	);
	return TextureId{ color_attachment.id };
}

V2_int Renderer::GetRenderTargetSize(RenderTargetId render_target) const {
	auto id{ GetRenderTargetTexture(render_target) };

	auto size{ gl_->textures.GetTextureSize(id) };

	return size;
}

TextureFormat Renderer::GetRenderTargetTextureFormat(RenderTargetId render_target) const {
	auto id{ GetRenderTargetTexture(render_target) };

	auto texture_format{ gl_->textures.GetTextureFormat(id) };

	return texture_format;
}

void Renderer::ClearRenderTarget(RenderTargetId render_target, Color color, bool set_viewport)
	const {
	auto bind_guard = gl_->Bind(FramebufferId{ render_target }, true);

	std::optional<Viewport> viewport;
	if (set_viewport) {
		viewport = gl_->GetViewport();

		auto render_target_size{ GetRenderTargetSize(render_target) };

		gl_->SetViewport({ .position{}, .size{ render_target_size } });
	}

	gl_->framebuffers.ClearToColor(FramebufferId{ render_target }, color);

	if (set_viewport && viewport.has_value()) {
		gl_->SetViewport(*viewport);
	}
}

void Renderer::BindRenderTarget(RenderTargetId render_target) {
	SetFramebuffer(FramebufferId{ render_target });
}

void Renderer::BindRenderPass(RenderPass& render_pass) {
	// Bind the next write target (opposite of latest output; ping for first write)
	RenderTargetId write;

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

	PTGN_ASSERT(current_pipeline_ != 0, "Current pipeline must be set");

	auto it{ std::ranges::find_if(pipelines_, [this](const auto& pair) {
		return pair.first == current_pipeline_;
	}) };

	PTGN_ASSERT(
		it != pipelines_.end(), "No matching current render pipeline found: ", current_pipeline_
	);

	const auto& pipeline{ it->second };

	auto _0{ gl_->Bind(pipeline.vao, false) };
	auto _1{ gl_->Bind(pipeline.vbo, false) };
	auto _2{ gl_->Bind(pipeline.ebo, false) };

	auto vertex_count{ static_cast<std::uint32_t>(batch_vertices_.size() / pipeline.vertex_size) };

	// Upload vertex data
	gl_->buffers.SetBufferSubData<VertexBufferId>(
		pipeline.vbo, gl::BufferTarget::ArrayBuffer, batch_vertices_.data(), 0, vertex_count,
		pipeline.vertex_size
	);

	// Upload index data
	gl_->buffers.SetBufferSubData<ElementBufferId>(
		pipeline.ebo, gl::BufferTarget::ElementArrayBuffer, batch_indices_.data(), 0,
		static_cast<std::uint32_t>(batch_indices_.size()), sizeof(Index)
	);

	// Bind all textures
	for (std::uint32_t slot{ 0 }; slot < batch_textures_.size(); ++slot) {
		gl_->SetActiveTextureSlot(slot);
		auto _3{ gl_->Bind(batch_textures_[slot], false) };
	}

	gl_->vertex_arrays.DrawElements(
		pipeline.vao, static_cast<std::uint32_t>(batch_indices_.size()), gl::IndexType::UnsignedInt,
		pipeline.primitive_mode
	);

	batch_vertices_.clear();
	batch_indices_.clear();
	batch_textures_.clear();
}

std::pair<std::uint32_t, bool> Renderer::GetTextureSlot(TextureId tex) {
	// Check if texture already exists in batch
	for (std::uint32_t i{ 0 }; i < batch_textures_.size(); ++i) {
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

template <typename State, InvocableR<void> F>
void UpdateStateIfChanged(
	Renderer& r, const std::optional<State>& cached, const State& desired, F&& func
) {
	if (cached != desired) {
		r.FlushBatch();
		std::invoke(std::forward<F>(func));
	}
}

void Renderer::SetPipeline(std::string_view name) {
	auto id{ Hash(name) };
	if (id == current_pipeline_) {
		return;
	}
	PTGN_ASSERT(
		(std::ranges::find_if(pipelines_, [id](const auto& pair) { return pair.first == id; }) !=
		 pipelines_.end()),
		"No matching pipeline found: ", id
	);
	FlushBatch();
	current_pipeline_ = id;
}

void Renderer::SetViewport(Viewport viewport) {
	UpdateStateIfChanged(*this, gl_->GetBoundState().viewport, viewport, [this, viewport] {
		gl_->SetViewport(viewport);
	});
}

void Renderer::SetViewProjection(const Matrix4& view_projection) {
	if (view_projection_ != view_projection) {
		FlushBatch();
		view_projection_ = view_projection;
	}
	// TODO: Find a better way to do this. This is needed to ensure that the shader's
	// uniform is updated even if the shader itself doesn't change.
	if (auto shader{ gl_->GetBoundShader() }; shader.has_value() && *shader) {
		gl_->shaders.SetUniform(*shader, "u_ViewProjection", view_projection_);
	}
}

void Renderer::SetShader(ShaderId shader) {
	UpdateStateIfChanged(*this, gl_->GetBoundState().shader_program, shader, [this, shader] {
		auto _ = gl_->Bind(shader, false);
		gl_->shaders.SetUniform(shader, "u_ViewProjection", view_projection_);
	});
}

void Renderer::SetBlend(bool enabled) {
	UpdateStateIfChanged(*this, gl_->GetBoundState().blend, enabled, [this, enabled] {
		gl_->SetBlend(enabled);
	});
}

void Renderer::SetBlendMode(BlendMode mode) {
	UpdateStateIfChanged(*this, gl_->GetBoundState().blend_mode, mode, [this, mode] {
		gl_->SetBlendMode(mode);
	});
}

void Renderer::SetFramebuffer(FramebufferId framebuffer) {
	UpdateStateIfChanged(*this, gl_->GetBoundState().framebuffer, framebuffer, [this, framebuffer] {
		auto _ = gl_->Bind(framebuffer, false);
	});
}

void Renderer::SetDepthTesting(bool enabled) {
	UpdateStateIfChanged(*this, gl_->GetBoundState().depth_testing, enabled, [this, enabled] {
		gl_->SetDepthTesting(enabled);
	});
}

void Renderer::SetDepthMask(const DepthMaskState& mask) {
	UpdateStateIfChanged(*this, gl_->GetBoundState().depth_mask, mask, [this, mask] {
		gl_->SetDepthMask(mask);
	});
}

void Renderer::SetStencil(const StencilState& stencil) {
	UpdateStateIfChanged(*this, gl_->GetBoundState().stencil, stencil, [this, stencil] {
		gl_->SetStencil(stencil);
	});
}

void Renderer::SetRaster(const RasterState& raster) {
	UpdateStateIfChanged(*this, gl_->GetBoundState().raster, raster, [this, raster] {
		gl_->SetRaster(raster);
	});
}

void Renderer::SetScissor(const ScissorState& scissor) {
	UpdateStateIfChanged(*this, gl_->GetBoundState().scissor, scissor, [this, scissor] {
		gl_->SetScissor(scissor);
	});
}

void Renderer::SetColorMask(const ColorMaskState& color_mask) {
	UpdateStateIfChanged(*this, gl_->GetBoundState().color_mask, color_mask, [this, color_mask] {
		gl_->SetColorMask(color_mask);
	});
}

void Renderer::FlushIfExceedsCapacity(
	std::size_t vertex_bytes, std::size_t indices, std::size_t vertex_byte_capacity
) {
	if (batch_vertices_.size() + vertex_bytes > vertex_byte_capacity ||
		batch_indices_.size() + indices > kIndexCapacity) {
		FlushBatch();
	}
}

ShaderId Renderer::GetShader(std::string_view name) const {
	return gl_->shaders.GetProgram(name);
}

bool Renderer::IsTextureAttachedToCurrentFramebuffer(TextureId texture) const {
	auto bound{ gl_->GetBoundFramebuffer() };

	if (!bound.has_value() || *bound == FramebufferId{ 0 }) {
		return false;
	}

	return gl_->framebuffers.GetFramebufferAttachment(*bound, gl::Attachment::Color0).id == texture;
}

void Renderer::DrawTriangle(
	ShaderId shader, const std::array<V2_float, 3>& positions, float depth, Color tint,
	int entity_id
) {
	SetPipeline("color");

	SetShader(shader);

	auto color_n{ tint.Normalized() };

	auto vertices{ GetVertices<ColorVertex, 3>([&](std::size_t i) {
		PTGN_ASSERT(i < positions.size());
		return ColorVertex{ positions[i], depth, color_n, entity_id };
	}) };

	constexpr std::array<Index, 3> indices{ 0, 1, 2 };

	SubmitVertices<ColorVertex>(vertices, indices);
}

void Renderer::DrawQuad(
	ShaderId shader, const std::array<V2_float, 4>& positions, float depth, Color tint,
	int entity_id
) {
	SetPipeline("color");

	SetShader(shader);

	auto color_n{ tint.Normalized() };

	auto vertices{ GetVertices<ColorVertex, 4>([&](std::size_t i) {
		PTGN_ASSERT(i < positions.size());
		return ColorVertex{ positions[i], depth, color_n, entity_id };
	}) };

	constexpr std::array<Index, 6> indices{ 0, 1, 2, 2, 3, 0 };

	SubmitVertices<ColorVertex>(vertices, indices);
}

void Renderer::DrawShape(
	ShaderId shader, const std::array<V2_float, 4>& positions, float depth, Color tint,
	const std::array<V2_float, 4>& tex_coords, const std::array<float, 4>& shape_data, int entity_id
) {
	SetPipeline("shape");

	PTGN_ASSERT(shader != 0);

	SetShader(shader);

	auto color_n{ tint.Normalized() };

	auto vertices{ GetVertices<ShapeVertex, 4>([&](std::size_t i) {
		PTGN_ASSERT(i < positions.size() && i < tex_coords.size());
		return ShapeVertex{ positions[i], depth, color_n, tex_coords[i], shape_data, entity_id };
	}) };

	constexpr std::array<Index, 6> indices{ 0, 1, 2, 2, 3, 0 };

	SubmitVertices<ShapeVertex>(vertices, indices);
}

void Renderer::DrawShader(
	ShaderId shader, const std::array<V2_float, 4>& positions, float depth, Color tint,
	const std::array<V2_float, 4>& tex_coords, const std::function<void()>& shader_setup,
	int entity_id
) {
	SetPipeline("texture");

	PTGN_ASSERT(shader != 0);

	SetShader(shader);

	if (shader_setup) {
		shader_setup();
	}

	auto color_n{ tint.Normalized() };

	auto vertices{ GetVertices<TextureVertex, 4>([&](std::size_t i) {
		PTGN_ASSERT(i < positions.size() && i < tex_coords.size());
		return TextureVertex{ positions[i], depth, color_n, tex_coords[i], 0.0f, entity_id };
	}) };

	constexpr std::array<Index, 6> indices{ 0, 1, 2, 2, 3, 0 };

	SubmitVertices<TextureVertex>(vertices, indices);

	FlushBatch();
}

void Renderer::DrawTexture(
	ShaderId shader, TextureId texture, const std::array<V2_float, 4>& positions, float depth,
	Color tint, const std::array<V2_float, 4>& tex_coords,
	const std::function<void()>& shader_setup, int entity_id
) {
	SetPipeline("texture");
	PTGN_ASSERT(
		!IsTextureAttachedToCurrentFramebuffer(texture),
		"Cannot draw a texture that is attached to the currently set framebuffer"
	);
	PTGN_ASSERT(shader != 0);
	PTGN_ASSERT(texture != 0);

	auto [slot, push_texture] = GetTextureSlot(texture);

	SetShader(shader);

	if (shader_setup) {
		shader_setup();
	}

	bool flush_after{ shader_setup != nullptr };

	auto color_n{ tint.Normalized() };

	auto vertices{ GetVertices<TextureVertex, 4>([&](std::size_t i) {
		PTGN_ASSERT(i < positions.size() && i < tex_coords.size());
		return TextureVertex{ positions[i], depth, color_n, tex_coords[i], static_cast<float>(slot),
							  entity_id };
	}) };

	constexpr std::array<Index, 6> indices{ 0, 1, 2, 2, 3, 0 };

	SubmitVertices<TextureVertex>(vertices, indices);

	if (push_texture) {
		batch_textures_.push_back(texture);
	}

	if (flush_after) {
		FlushBatch();
	}
}

void Renderer::OnWindowResize(V2_int size) {
	if (presentation_viewport_.has_value()) {
		return;
	}

	if (!game_size_.has_value()) {
		event_sink_(size, ResizeType::Game);
	}

	event_sink_(size, impl::PresentationResizeType{});

	display_viewport_dirty_ = true;
}

void Renderer::SetGameSize(
	std::optional<V2_int> game_size, std::optional<ScalingMode> scaling_mode
) {
	if (game_size_ == game_size &&
		(!scaling_mode.has_value() || scaling_mode.has_value() && scaling_mode_ == scaling_mode)) {
		return;
	}

	PTGN_ASSERT(
		!game_size.has_value() || game_size.has_value() && game_size->BothAboveZero(),
		"Game size cannot be set to negative value or zero"
	);

	game_size_ = game_size;
	if (scaling_mode.has_value()) {
		scaling_mode_ = *scaling_mode;
	}

	auto size{ GetGameSize() };

	event_sink_(size, ResizeType::Game);

	display_viewport_dirty_ = true;
}

void Renderer::SetScalingMode(ScalingMode scaling_mode) {
	if (scaling_mode_ == scaling_mode) {
		return;
	}

	scaling_mode_ = scaling_mode;

	display_viewport_dirty_ = true;
}

void Renderer::SetPresentationViewport(std::optional<Viewport> presentation_viewport) {
	if (presentation_viewport_ == presentation_viewport) {
		return;
	}
	presentation_viewport_ = presentation_viewport;

	if (!presentation_viewport_.has_value()) {
		OnWindowResize(GetFullViewportSize());
		return;
	}

	if (!game_size_.has_value()) {
		event_sink_(presentation_viewport_->size, ResizeType::Game);
	}

	event_sink_(presentation_viewport_->size, impl::PresentationResizeType{});
	display_viewport_dirty_ = true;
}

bool Renderer::HasGameSize() const {
	return game_size_.has_value();
}

V2_int Renderer::GetGameSize() const {
	if (HasGameSize()) {
		return *game_size_;
	}
	return GetPresentationSize();
}

ScalingMode Renderer::GetScalingMode() const {
	return scaling_mode_;
}

Viewport Renderer::GetPresentationViewport() const {
	return { .position{ GetPresentationPosition() }, .size{ GetPresentationSize() } };
}

V2_int Renderer::GetPresentationPosition() const {
	if (presentation_viewport_.has_value()) {
		return presentation_viewport_->position;
	}
	return { 0, 0 };
}

V2_int Renderer::GetPresentationSize() const {
	if (presentation_viewport_.has_value()) {
		return presentation_viewport_->size;
	}
	return GetFullViewportSize();
}

Viewport Renderer::GetDisplayViewport() const {
	return display_viewport_;
}

V2_int Renderer::GetDisplayPosition() const {
	return display_viewport_.position;
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

V2_int Renderer::GetFullViewportSize() const {
	return window_.GetSize();
}

void Renderer::SetBackgroundColor(Color background_color) {
	background_color_ = background_color;
}

Color Renderer::GetBackgroundColor() const {
	return background_color_;
}

void Renderer::SetPrimaryWorldCamera(const std::optional<Camera>& primary_world_camera) {
	primary_world_camera_ = primary_world_camera;
}

const std::optional<Camera>& Renderer::GetPrimaryWorldCamera() const {
	return primary_world_camera_;
}

void Renderer::UpdateDisplayViewport(bool emit_events) {
	if (!display_viewport_dirty_) {
		return;
	}

	auto resize_info = RecalculateDisplayViewport();

	display_viewport_dirty_ = false;

	if (!resize_info.moved && !resize_info.resized) {
		return;
	}

	display_viewport_ = resize_info.viewport;

	if (resize_info.resized) {
		ResizeScreenTarget(display_viewport_.size);

		if (emit_events) {
			event_sink_(display_viewport_.size, ResizeType::Display);
		}
	}
}

Renderer::DisplayResizeInfo Renderer::RecalculateDisplayViewport() const {
	const auto presentation{ GetPresentationViewport() };

	PTGN_ASSERT(presentation.size.BothAboveZero());

	auto game_size{ game_size_.value_or(presentation.size) };

	PTGN_ASSERT(game_size.BothAboveZero());

	Viewport viewport{ .position{}, .size{ presentation.size } };

	auto compute_aspect_fit = [&viewport, game_size, presentation](bool letterbox_mode) {
		float presentation_aspect{ static_cast<float>(presentation.size.x) / presentation.size.y };
		float game_aspect{ static_cast<float>(game_size.x) / game_size.y };

		// In letterbox mode we need require presentation_aspect > game_aspect to fit
		// height, and in overscan we require presentation_aspect > game_aspect to fit
		// height.
		bool fit_height{ (presentation_aspect > game_aspect) == letterbox_mode };

		if (fit_height) {
			viewport.size.y = presentation.size.y;
			viewport.size.x =
				static_cast<int>(static_cast<float>(presentation.size.y) * game_aspect + 0.5f);
			viewport.position.x = (presentation.size.x - viewport.size.x) / 2; // left edge.
			viewport.position.y = 0;
		} else {
			// Fit width.
			viewport.size.x = presentation.size.x;
			viewport.size.y =
				static_cast<int>(static_cast<float>(presentation.size.x) / game_aspect + 0.5f);
			viewport.position.x = 0;
			viewport.position.y = (presentation.size.y - viewport.size.y) / 2; // top edge.
		}
	};

	switch (scaling_mode_) {
		case ScalingMode::Letterbox: compute_aspect_fit(true); break;
		case ScalingMode::Overscan:	 compute_aspect_fit(false); break;

		case ScalingMode::Stretch:
			PTGN_ASSERT(viewport.size == presentation.size);
			PTGN_ASSERT(viewport.position == V2_int{});
			// Viewport is full presentation area (default).
			break;

		case ScalingMode::IntegerScale: {
			V2_int ratio{ presentation.size / game_size };
			// Find which dimension limits the scaling factor.
			int scale{ std::max(1, std::min(ratio.x, ratio.y)) };
			viewport.size = game_size * scale;			 // scale up.
			viewport.position =
				(presentation.size - viewport.size) / 2; // center of presentation viewport.
			break;
		}

		case ScalingMode::Disabled:
			viewport.size = game_size;					 // no change.
			viewport.position =
				(presentation.size - viewport.size) / 2; // center of presentation viewport.
			break;

		default: PTGN_ERROR("Unsupported resolution mode");
	}

	bool resized{ viewport.size != display_viewport_.size };
	bool moved{ viewport.position != display_viewport_.position };

	PTGN_ASSERT(viewport.size.BothAboveZero());

	return { .moved = moved, .resized = resized, .viewport{ viewport } };
}

void Renderer::ResizeScreenTarget(V2_int size) {
	ResizeRenderTarget(screen_target_.resource_, size);
}

void Renderer::BindScreenTarget() {
	BindRenderTarget(screen_target_);
}

RenderTargetId Renderer::GetScreenTarget() const {
	return screen_target_.resource_;
}

void Renderer::InvalidateState() {
	gl_->InvalidateState();
}

void Renderer::BeginFrame() {
	InvalidateState();

	PTGN_ASSERT(batch_vertices_.empty());
	PTGN_ASSERT(batch_indices_.empty());

	if (!presentation_viewport_.has_value()) {
		auto presentation{ GetPresentationViewport() };
		Color window_background_color{ window_.GetBackgroundColor() };

		auto _1 = gl_->Bind(FramebufferId{ 0 }, false);
		gl_->SetClearColor(window_background_color);
		SetViewport(presentation);
		gl_->framebuffers.Clear();
	}

	BindScreenTarget();
	SetViewport({ .position{}, .size{ screen_target_.GetSize() } });
	gl_->framebuffers.ClearToColor(FramebufferId{ screen_target_.resource_ }, background_color_);
}

void Renderer::EndFrame() {
	PTGN_ASSERT(display_viewport_.size.BothAboveZero());

	FlushBatch();

	SetFramebuffer({});

	if (presentation_viewport_.has_value()) {
		return;
	}

	V2_float half_viewport{ display_viewport_.size * 0.5f };
	SetViewport(display_viewport_);
	auto view_projection{ Matrix4::Orthographic(-half_viewport, half_viewport) };
	SetViewProjection(view_projection);
	SetBlendMode(BlendMode::ReplaceRGBA);

	PTGN_ASSERT(
		GetRenderTargetSize(screen_target_.resource_) == display_viewport_.size,
		"Screen target texture size must match display viewport size"
	);
	auto texture_shader{ GetShader("texture") };
	auto points{ GetCenteredQuadPoints(display_viewport_.size) };
	auto tex_coords{ GetDefaultTextureCoordinates<true>() };

	auto screen_texture{ GetRenderTargetTexture(screen_target_.resource_) };

	DrawTexture(texture_shader, screen_texture, points, 0.0f, color::White, tex_coords, {}, -1);

	FlushBatch();
}

bool Renderer::IsPresentationViewportVisible() const {
	return presentation_viewport_.has_value() && !presentation_viewport_->size.BothAboveZero();
}

ShaderObject Renderer::CreateShader(
	const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
) {
	return ShaderObject{ this, gl_->shaders.CreateProgram(source, shader_name) };
}

TextureObject Renderer::CreateTexture(
	const std::uint8_t* pixel_data, V2_int size, TextureFormat format
) {
	auto [pixel_format, pixel_type] = gl::GetPixelDataFormat(format);
	PTGN_ASSERT(
		pixel_type == gl::PixelDataType::UnsignedByte, "Texture format must have a type of bytes"
	);
	return TextureObject{
		this, gl_->textures.CreateTexture(pixel_data, pixel_format, pixel_type, size, format)
	};
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, const Matrix4& v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V2_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V3_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V4_float v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, const std::vector<float>& v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V2_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V3_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, V4_int v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, const std::vector<int>& v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

void Renderer::SetUniform(ShaderId shader, const char* uniform_name, bool v) {
	gl_->shaders.SetUniform(shader, uniform_name, v);
}

ElementBufferObject Renderer::CreateElementBufferObject(std::uint32_t index_capacity) {
	return ElementBufferObject{ this, gl_->buffers.CreateElementBuffer(
										  nullptr, index_capacity, sizeof(Index),
										  gl::BufferUsage::DynamicDraw
									  ) };
}

VertexBufferObject Renderer::CreateVertexBufferObject(
	std::uint32_t vertex_capacity, std::uint32_t vertex_size
) {
	return VertexBufferObject{ this, gl_->buffers.CreateVertexBuffer(
										 nullptr, vertex_capacity, vertex_size,
										 gl::BufferUsage::DynamicDraw
									 ) };
}

VertexArrayObject Renderer::CreateVertexArrayObject(
	VertexBufferId vertex_buffer, const BufferLayoutView& layout, ElementBufferId element_buffer
) {
	return VertexArrayObject{
		this, gl_->vertex_arrays.CreateVertexArray(vertex_buffer, layout, element_buffer)
	};
}

void Renderer::Destroy(VertexBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(ElementBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(UniformBufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(ShaderId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(TextureId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(RenderbufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(FramebufferId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(VertexArrayId id) {
	gl_->Destroy(id);
}

void Renderer::Destroy(RenderTargetId id) {
	gl_->Destroy(id);
};

V2_int Renderer::GetTextureSize(TextureId texture) const {
	return gl_->textures.GetTextureSize(texture);
}

TextureFormat Renderer::GetTextureFormat(TextureId texture) const {
	return gl_->textures.GetTextureFormat(texture);
}

void Renderer::ResizeRenderTarget(RenderTargetId render_target, V2_int new_size) {
	gl_->framebuffers.ResizeFramebuffer(FramebufferId{ render_target }, new_size);
}

} // namespace ptgn::impl