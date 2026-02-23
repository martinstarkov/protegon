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

Renderer::Renderer(Window& window) : gl{ std::make_unique<GLContext>(window) } {
	// TODO: Fix and replace with the object which has a destructor.
	ebo_ = ElementBufferObject{ this,
								gl->buffers.CreateElementBuffer(
									nullptr, index_capacity, sizeof(Index), BufferUsage::DynamicDraw
								) };

	vbo_ = VertexBufferObject{ this, gl->buffers.CreateVertexBuffer(
										 nullptr, vertex_capacity, sizeof(Vertex),
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

Renderer::~Renderer() noexcept {
	// Needs to have access to GLContext destructor, forward declaration is not enough.
}

RenderTarget Renderer::CreateRenderTarget(V2_int size, TextureFormat format) {
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

	return RenderTarget{ this, RenderTargetData{ framebuffer, color, depth, size, format } };
}

RenderPass Renderer::BeginPass(const RenderTargetData& scene_target) {
	RenderPass p;
	p.source_			= scene_target;
	p.ping_				= AcquirePooledTarget(scene_target.size_, scene_target.format_);
	p.has_ping_			= true;
	p.has_written_once_ = false; // latest = source initially
	p.latest_is_ping_	= true;	 // irrelevant until has_written_once==true

	return p;
}

V2_int Renderer::GetTextureSize(TextureId texture) const {
	return gl->textures.GetTextureSize(texture);
}

void Renderer::FlushBatch() {
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
		vao_, static_cast<std::uint32_t>(batch_indices_.size()), GL_UNSIGNED_INT, GL_TRIANGLES
	);

	// Clear batch (keep white texture)
	batch_vertices_.clear();
	batch_indices_.clear();
	batch_textures_.resize(1);
	batch_textures_[0] = white_texture_;
}

RenderTargetData Renderer::AcquirePooledTarget(V2_int size, TextureFormat format) {
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

void Renderer::ReleasePooledTarget(RenderTargetData& target) {
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

std::uint32_t Renderer::GetTextureSlot(TextureId tex) {
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
void UpdateStateIfChanged(Renderer& r, const State& cached, const State& desired, Func&& func) {
	if (cached != desired) {
		r.FlushBatch();
		std::invoke(std::forward<Func>(func));
	}
}

void Renderer::SetViewProjection(const Matrix4& view_projection) {
	if (view_projection_ != view_projection) {
		view_projection_ = view_projection;
		FlushBatch();
	}
}

void Renderer::SetShader(ShaderId shader) {
	UpdateStateIfChanged(*this, gl->GetBoundState().shader_program, shader, [this, shader] {
		auto _ = gl->Bind(shader);
	});
}

void Renderer::SetBlend(BlendMode mode, bool enabled) {
	BlendState desired{ mode, enabled };

	UpdateStateIfChanged(*this, gl->GetBoundState().blend, desired, [this, desired] {
		gl->SetBlend(desired);
	});
}

void Renderer::SetFramebuffer(FramebufferId framebuffer, const Viewport& viewport) {
	UpdateStateIfChanged(*this, gl->GetBoundState().framebuffer, framebuffer, [this, framebuffer] {
		auto _ = gl->Bind(framebuffer);
	});
	gl->SetViewport(viewport);
}

void Renderer::SetDepth(const DepthState& depth) {
	UpdateStateIfChanged(*this, gl->GetBoundState().depth, depth, [this, depth] {
		gl->SetDepth(depth);
	});
}

void Renderer::SetStencil(const StencilState& stencil) {
	UpdateStateIfChanged(*this, gl->GetBoundState().stencil, stencil, [this, stencil] {
		gl->SetStencil(stencil);
	});
}

void Renderer::SetRaster(const RasterState& raster) {
	UpdateStateIfChanged(*this, gl->GetBoundState().raster, raster, [this, raster] {
		gl->SetRaster(raster);
	});
}

void Renderer::SetColorMask(const ColorMaskState& color_mask) {
	UpdateStateIfChanged(*this, gl->GetBoundState().color_mask, color_mask, [this, color_mask] {
		gl->SetColorMask(color_mask);
	});
}

void Renderer::DrawQuad(ShaderId shader, const QuadParams& params, const QuadSetup& setup) {
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

	SetShader(shader);
	gl->shaders.SetUniform(shader, "u_ViewProjection", view_projection_);

	setup(shader, quad);

	auto vertices{
		Vertex::GetQuad(quad.positions, quad.color, quad.depth, quad.user_data, quad.tex_coords)
	};

	constexpr std::array<Index, 6> indices{ 0, 1, 2, 2, 3, 0 };

	if (batch_vertices_.size() + vertices.size() >= vertex_capacity ||
		batch_indices_.size() + indices.size() >= index_capacity) {
		FlushBatch();
	}

	auto start_index = static_cast<std::uint32_t>(batch_vertices_.size());

	batch_vertices_.insert(batch_vertices_.end(), vertices.begin(), vertices.end());

	for (auto idx : indices) {
		batch_indices_.push_back(idx + start_index);
	}
}

ShaderId Renderer::GetShader(std::string_view name) const {
	return gl->shaders.GetProgram(name);
}

void Renderer::DrawTexture(
	ShaderId shader, TextureId texture, const std::array<V2_float, 4>& positions, Color tint,
	float depth, bool flip_y, const std::optional<std::array<V2_float, 4>>& tex_coords
) {
	PTGN_ASSERT(
		gl->GetBoundFramebuffer() == FramebufferId{ 0 } ||
			TextureId{ gl->framebuffers
						   .GetFramebufferAttachment(gl->GetBoundFramebuffer(), Attachment::Color0)
						   .id } != texture,
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

void Renderer::DrawTexture(ShaderId shader, RenderPass& p, const RenderTargetData& scene_target) {
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

		write.Bind(*gl);

		DrawTexture(
			shader, *input.color_,
			GetCenteredQuadPoints(gl->textures.GetTextureSize(*input.color_)), color::White, 0.0f,
			flip_y
		);

		// Update pass state
		p.has_written_once_ = true;
		p.latest_is_ping_	= (write.framebuffer_ == p.ping_.framebuffer_);
	} else {
		// Read-only draw: no mutation, no flip
		DrawTexture(
			shader, *input.color_,
			GetCenteredQuadPoints(gl->textures.GetTextureSize(*input.color_)), color::White, 0.0f,
			flip_y
		);
	}
}

void Renderer::BeginFrame() {
	PTGN_ASSERT(batch_vertices_.empty());
	PTGN_ASSERT(batch_indices_.empty());

	auto _1 = gl->Bind(FramebufferId{ 0 });
	gl->SetClearColor(color::Transparent);
	gl->framebuffers.Clear();

	screen_target_.Bind();
	gl->framebuffers.ClearToColor(screen_target_.resource_.framebuffer_, color::Transparent);
}

void Renderer::EndFrame(const Viewport& viewport) {
	PTGN_ASSERT(viewport.size.BothAboveZero());

	SetFramebuffer({}, viewport);

	auto half_viewport{ viewport.size * 0.5f };
	SetViewProjection(Matrix4::Orthographic(-half_viewport, half_viewport));
	SetBlend(BlendMode::ReplaceRGBA);

	PTGN_ASSERT(
		screen_target_.resource_.color_.has_value(),
		"Cannot draw to screen target with no color attachment"
	);

	DrawTexture(
		GetShader("quad"), *screen_target_.resource_.color_,
		GetCenteredQuadPoints(screen_target_.resource_.size_), color::White, 0.0f, true
	);

	FlushBatch();
}

TextureId Renderer::GetWhiteTexture() const {
	return white_texture_;
}

const RenderTargetData& Renderer::GetScreenTarget() const {
	return screen_target_.resource_;
}

RenderTargetData& Renderer::GetScreenTarget() {
	return screen_target_.resource_;
}

} // namespace ptgn::impl::gl

/*

bool Renderer::IsTextureAttachedToCurrentFramebuffer(
	Texture tex
) const {
	if (!state.framebuffer) {
		return false; // default framebuffer
	}

	const auto& fb = state.framebuffer;

	for (GLenum attachment : gl->GetFramebufferColorAttachments(fb)) {
		const auto& info = gl->GetFramebufferAttachment(fb, attachment);
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

	const auto& tex_info = gl->GetTextureInfo(src);

	entry.texture = gl->CreateTexture2D(
		tex_info.size,
		tex_info.internal_format,
		tex_info.filter,
		tex_info.wrap
	);

	entry.fbo = gl->CreateFramebuffer();
	gl->AttachTexture(entry.fbo, GL_COLOR_ATTACHMENT0, entry.texture);

	PTGN_ASSERT(gl->CheckFramebufferComplete(entry.fbo));

	return entry;
}

void Renderer::ResolveReadWriteHazards() {
	if (!state.framebuffer) {
		return; // default framebuffer, no hazard
	}

	for (std::uint32_t i = 0; i < batch_textures_.size(); ++i) {
		auto tex = batch_textures_[i];

		if (!IsTextureAttachedToCurrentFramebuffer(tex)) {
			continue;
		}

		// Hazard detected
		auto& pp = GetPingPongFor(tex);

		// Flush pending geometry before redirecting
		FlushBatch();

		// Blit tex -> pingpong
		gl->BlitTexture(tex, pp.texture);

		// Replace read texture in batch
		batch_textures_[i] = pp.texture;

		// IMPORTANT: future writes now go to the pingpong target
		// so swap framebuffer attachment
		gl->ReplaceFramebufferAttachment(
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
	if (batch_indices_.empty()) {
		return;
	}

	ResolveReadWriteHazards();

	...
}


*/

/*
RecomputeDisplaySize(window_.GetSize());

// GLRenderer::EnableLineSmoothing();

GLRenderer::DisableDepthTesting();
GLRenderer::DisableGammaCorrection();

max_texture_slots = GLRenderer::GetMaxTextureSlots();

PTGN_INFO("Renderer Texture Slots: ", max_texture_slots);

const auto& screen_shader{ gl->GetShader("screen_default") };
PTGN_ASSERT(screen_shader);
gl->Bind(screen_shader);
gl->SetUniform(screen_shader, "u_Texture", 1);

const auto& quad_shader{ gl->GetShader("quad") };

PTGN_ASSERT(quad_shader);
PTGN_ASSERT(gl->GetShader("circle"));
PTGN_ASSERT(gl->GetShader("screen_default"));
PTGN_ASSERT(gl->GetShader("light"));

intermediate_target = {};

screen_target__ = CreateRenderTarget(
	render_manager, display_viewport_.size, color::Transparent, TextureFormat::RGBA8888, true
);
AddScript<DisplayResizeScript>(screen_target__);

SetBlendMode(screen_target__, BlendMode::ReplaceRGBA);

#ifdef PTGN_PLATFORM_MACOS
// Prevents MacOS warning: "UNSUPPORTED (log once): POSSIBLE ISSUE: unit X
// GLD_TEXTURE_INDEX_2D is unloadable and bound to sampler type (Float) - using zero
// texture because texture unloadable."
for (std::uint32_t slot{ 0 }; slot < max_texture_slots; slot++) {
	Texture::Bind(white_texture_.GetId(), slot);
}
#endif

SetState(RenderState{ {}, BlendMode::ReplaceRGBA, {} });

viewport_tracker = render_manager.CreateEntity();
AddScript<ViewportResizeScript>(viewport_tracker, ctx_);
auto window_size{ window_.GetSize() };
RecomputeDisplaySize(window_size);

render_manager.Refresh();
*/

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

void Renderer::DrawCommand(const DrawCommand& cmd) {
	std::visit(
		[&](const auto& command) {
			using T = std::decay_t<decltype(command)>;

			if constexpr (std::is_same_v<T, DrawShapeCommand>) {
				std::visit(
					[&](const auto& shape) { DrawShape(*this, command, shape); },
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

		white_texture_.Bind(0);
		force_flush = true;
	}

	float texture_index = 0.0f;

	auto get_texture_index = [&](TextureId id, float& out_texture_index) {
		PTGN_ASSERT(id != white_texture_.GetId());
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

		PTGN_ASSERT(texture, "Cannot draw shader to an invalid texture");

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
		gl->Bind(shader_pass.shader);
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

		gl->Bind(shader);
		gl->SetUniform(shader, "u_Texture", 1);
		gl->SetUniform(shader, "u_ViewportSize", V2_float{ target.viewport.size });

		if (shader_pass.uniform_callback) {
			PTGN_ASSERT(shader_pass.shader);
			gl->Bind(shader_pass.shader);
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
		gl->SetActiveTextureSlot(slot);
		gl->Bind(textures[i]);
	}

	gl->DrawElements(triangle_vao, indices.size(), GL_TRIANGLES);
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
		std::vector<DrawCommand>& commands{ it->second };

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
	screen_target__.Clear();
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

	const auto& texture{ screen_target__.GetTexture() };

	DrawCall(
		GetFullscreenShader(texture.GetFormat()),
		Vertex::GetQuad(
			{ -half_viewport, V2_float{ half_viewport.x, -half_viewport.y }, half_viewport,
			  V2_float{ -half_viewport.x, half_viewport.y } },
			GetTint(screen_target__), GetDepth(screen_target__), { 1.0f },
			GetDefaultTextureCoordinates(), true
		),
		quad_indices, { texture.GetId() }, nullptr, false, color::Transparent,
		GetBlendMode(screen_target__), display_viewport_,
		Matrix4::Orthographic(-half_viewport, half_viewport)
	);
}

void Renderer::Draw(Scene& scene) {
	// PTGN_LOG(draw_context_pool.contexts_.size());
	// PTGN_PROFILE_FUNCTION();

	white_texture_.Bind(0);

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
		quad_indices, { texture.GetId() }, &screen_target__.GetFramebuffer(), false,
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

Texture Renderer::CreateTexture(
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
	render_data_.Submit(EnableStencilMask{});
}

void Renderer::DisableStencilMask() {
	render_data_.Submit(DisableStencilMask{});
}

void Renderer::DrawOutsideStencilMask() {
	render_data_.Submit(DrawOutsideStencilMask{});
}

void Renderer::DrawInsideStencilMask() {
	render_data_.Submit(DrawInsideStencilMask{});
}

void Renderer::SetBackgroundColor(Color background_color) {
	render_data_.screen_target__.SetClearColor(background_color);
}

Color Renderer::GetBackgroundColor() const {
	return render_data_.screen_target__.GetClearColor();
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