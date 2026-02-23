
#include "renderer/renderer.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string_view>

#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "platform/input/events.h"
#include "platform/window/window.h"
#include "renderer/backend/gl/gl_renderer.h"
#include "renderer/camera/scaling_mode.h"
#include "renderer/camera/viewport.h"
#include "renderer/resources/render_state.h"
#include "renderer/resources/render_target.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/event/event_handler.h"

namespace ptgn {

Renderer::Renderer(Window& window, EventHandler& events) :
	window_{ window },
	events_{ events },
	gl_renderer_{ std::make_shared<impl::gl::Renderer>(window) } {}

Renderer::~Renderer() noexcept {
	// Destructor access to impl::gl::Renderer is needed.
}

void Renderer::OnEvent(EventDispatcher d) {
	d.Dispatch<WindowResized>([this](auto& e) {
		if (!game_size_) {
			GameResized game_resized;
			game_resized.size = e.size;
			// PTGN_LOG("Emitting game resized: ", e.size);
			events_.Emit(game_resized);
		}

		UpdateDisplayViewport(e.size);
	});
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

	game_size_	  = game_size;
	scaling_mode_ = scaling_mode;

	GameResized game_resized;
	game_resized.size = GetGameSize();
	// PTGN_LOG("Emitting game resized: ", game_resized.size);
	events_.Emit(game_resized);
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

void Renderer::DrawTexture(
	impl::ShaderId shader, impl::TextureId texture, const std::array<V2_float, 4>& positions,
	Color tint, float depth, bool flip_y, const std::optional<std::array<V2_float, 4>>& tex_coords
) {
	gl_renderer_->DrawTexture(shader, texture, positions, tint, depth, flip_y, tex_coords);
}

void Renderer::DrawQuadTexture(
	impl::TextureId texture, const std::array<V2_float, 4>& positions, Color tint, float depth,
	bool flip_y, const std::optional<std::array<V2_float, 4>>& tex_coords
) {
	DrawTexture(GetShader("quad"), texture, positions, tint, depth, flip_y, tex_coords);
}

void Renderer::DrawQuad(
	const std::array<V2_float, 4>& positions, Color tint, float depth,
	const std::optional<std::array<V2_float, 4>>& tex_coords
) {
	DrawQuadTexture(GetWhiteTexture(), positions, tint, depth, false, tex_coords);
}

impl::TextureId Renderer::GetWhiteTexture() const {
	return gl_renderer_->GetWhiteTexture();
}

impl::ShaderId Renderer::GetShader(std::string_view name) const {
	return gl_renderer_->GetShader(name);
}

RenderTarget Renderer::CreateRenderTarget(V2_int size, TextureFormat format) {
	return gl_renderer_->CreateRenderTarget(size, format);
}

impl::RenderTargetData Renderer::GetScreenTarget() const {
	return gl_renderer_->GetScreenTarget();
}

void Renderer::SetViewProjection(const Matrix4& view_projection) {
	gl_renderer_->SetViewProjection(view_projection);
}

void Renderer::SetBlend(BlendMode mode, bool enabled) {
	gl_renderer_->SetBlend(mode, enabled);
}

void Renderer::SetDepth(const DepthState& depth) {
	gl_renderer_->SetDepth(depth);
}

void Renderer::SetStencil(const StencilState& stencil) {
	gl_renderer_->SetStencil(stencil);
}

void Renderer::SetRaster(const RasterState& raster) {
	gl_renderer_->SetRaster(raster);
}

void Renderer::SetColorMask(const ColorMaskState& color_mask) {
	gl_renderer_->SetColorMask(color_mask);
}

impl::RenderPass Renderer::BeginPass(const impl::RenderTargetData& scene_target) {
	return gl_renderer_->BeginPass(scene_target);
}

Viewport Renderer::GetDisplayViewport() const {
	return display_viewport_;
}

void Renderer::BeginFrame() {
	gl_renderer_->BeginFrame();
}

void Renderer::EndFrame() {
	gl_renderer_->EndFrame(display_viewport_);
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
			gl_renderer_->GetScreenTarget().Resize(*gl_renderer_->gl, display_viewport_.size);

			if (emit_events) {
				impl::DisplayResized display_resized;
				display_resized.size = display_viewport_.size;
				// PTGN_LOG("Emitting display resized: ", display_resized.size);
				events_.Emit(display_resized);
			}
		}

		if (emit_events) {
			impl::DisplayViewportChanged display_changed;
			display_changed.viewport = display_viewport_;
			// PTGN_LOG("Emitting viewport changed: ", display_changed.viewport);
			events_.Emit(display_changed);
		}
	}
}

} // namespace ptgn

// TODO: Fix.
/*

void DrawTexture(Renderer& renderer, Entity entity, bool flip_texture) {
	Sprite sprite{ entity };

	renderer.DrawTexture(
		sprite.Get<Handle<Texture>>(), GetDrawTransform(entity), sprite.GetSize(),
		GetDrawOrigin(entity), GetTint(entity), GetDepth(entity), GetBlendMode(entity),
		entity.GetOrDefault<Camera>(), entity.GetOrDefault<PreFX>(), entity.GetOrDefault<PostFX>(),
		sprite.GetTextureCoordinates(flip_texture)
	);
}

 void DrawText(
	Text text, const V2_int& text_size, const Camera& camera, const Color& additional_tint,
	Origin offset_origin, V2_float offset_size
) {
	if (!text.Has<TextContent>()) {
		return;
	}

	if (text.Get<TextContent>().GetValue().empty()) {
		return;
	}

	if (text.Has<TextColor>() && text.Get<TextColor>().a == 0) {
		return;
	}

	Tint tint{ GetTint(text) };
	Transform transform{ GetDrawTransform(text) };
	Camera cam{ text.GetOrDefault<Camera>() };

	if (tint.a == 0 || additional_tint.a == 0) {
		return;
	}

	if (camera) {
		cam = camera;
	}

	// Offset text so it is centered on the offset origin and size.
	auto offset{ -GetOriginOffset(offset_origin, offset_size * Abs(transform.GetScale())) };
	transform.Translate(offset);

	if (bool is_hd{ text.IsHD() }) {
		auto scene_scale{ text.GetScene().GetRenderTargetScaleRelativeTo(cam) };

		PTGN_ASSERT(scene_scale.BothAboveZero());

		transform.Scale(transform.GetScale() / scene_scale);

		if (text.GetFontSize(is_hd, cam) != text.Get<impl::CachedFontSize>()) {
			text.RecreateTexture(cam);
		}
	}

	const auto& text_texture{ text.GetTexture() };

	if (!text_texture) {
		return;
	}

	V2_int size{ text_size };

	// If the text texture size for any text_size dimension that is zero.
	if (size.HasZero()) {
		V2_int texture_size{ text_texture.GetSize() };
		if (!size.x) {
			size.x = texture_size.x;
		}
		if (!size.y) {
			size.y = texture_size.y;
		}
	}

	auto texture_coordinates{ Sprite{ text }.GetTextureCoordinates(false) };

	Color text_tint{ additional_tint.Normalized() * tint.Normalized() };

	Application::Get().render_.DrawTexture(
		text_texture, transform, size, GetDrawOrigin(text), text_tint, GetDepth(text),
		GetBlendMode(text), cam, text.GetOrDefault<PreFX>(), text.GetOrDefault<PostFX>(),
		texture_coordinates
	);
 }


template <ShapeType T>
static void DrawShape(Renderer& renderer, Entity entity) {
	PTGN_ASSERT(entity.Has<T>(), "Entity does not have shape: ", type_name<T>());

	Origin origin{ Origin::Center };

	if constexpr (IsAnyOf<T, Rect, RoundedRect>) {
		origin = GetDrawOrigin(entity);
	}

	const auto& shape{ entity.Get<T>() };

	renderer.DrawShape(
		GetDrawTransform(entity), shape, GetTint(entity), entity.GetOrDefault<LineWidth>(), origin,
		GetDepth(entity), GetBlendMode(entity), entity.GetOrDefault<Camera>(),
		entity.GetOrDefault<PostFX>(), entity.GetOrDefault<ShaderPass>()
	);
}

*/

// void Renderer::BindRenderTarget(RenderPass& p) {
//	// Bind the next write target (opposite of latest output; ping for first write)
//	RenderTarget write;
//
//	if (!p.has_written_once) {
//		write = p.ping;
//	} else {
//		if (!p.has_pong && p.latest_is_ping) {
//			p.pong	   = AcquirePooledTarget(p.source.size, p.source.format);
//			p.has_pong = true;
//		}
//		write = p.latest_is_ping ? p.pong : p.ping;
//	}
//
//	BindRenderTarget(write);
// }
//
// void Renderer::DrawTexture(
//	impl::gl::ShaderId shader, RenderPass& p, const RenderTarget& scene_target
//) {
//	RenderTarget input;
//
//	// Input = latest output, or source before first draw
//	if (!p.has_written_once) {
//		input = p.source;
//	} else if (p.latest_is_ping) {
//		input = p.ping;
//	} else {
//		input = p.pong;
//	}
//
//	// Are we rendering *into this pass*?
//	bool writing_to_pass = state.framebuffer == p.ping.framebuffer ||
//						   (p.has_pong && state.framebuffer == p.pong.framebuffer);
//
//	bool input_is_offscreen = input.framebuffer != scene_target.framebuffer;
//
//	bool output_is_offscreen = state.framebuffer != scene_target.framebuffer;
//
//	bool flip_y = input_is_offscreen && !output_is_offscreen;
//
//	// Only ping-pong if we're writing into the pass
//	if (writing_to_pass) {
//		RenderTarget write;
//
//		if (!p.has_written_once) {
//			write = p.ping;
//		} else {
//			if (!p.has_pong && p.latest_is_ping) {
//				p.pong	   = AcquirePooledTarget(p.source.size, p.source.format);
//				p.has_pong = true;
//			}
//			write = p.latest_is_ping ? p.pong : p.ping;
//		}
//
//		BindRenderTarget(write);
//
//		DrawTexturedQuad(
//			shader, input.color, { 0, 0 }, gl_->GetTextureSize(input.color), color::White, flip_y
//		);
//
//		// Update pass state
//		p.has_written_once = true;
//		p.latest_is_ping   = (write.framebuffer == p.ping.framebuffer);
//	} else {
//		// Read-only draw: no mutation, no flip
//		DrawTexturedQuad(
//			shader, input.color, { 0, 0 }, gl_->GetTextureSize(input.color), color::White, flip_y
//		);
//	}
// }
//
// void Renderer::FlushBatch() {
//	if (batch_indices.empty()) {
//		return; // Nothing to draw
//	}
//
//	auto _vao = gl_->Bind(vao);
//
//	// Upload vertex data
//	gl_->SetBufferSubData<impl::gl::VertexBufferId>(
//		vbo, GL_ARRAY_BUFFER, batch_vertices.data(), 0,
//		static_cast<std::uint32_t>(batch_vertices.size()), sizeof(impl::Vertex)
//	);
//
//	// Upload index data
//	gl_->SetBufferSubData<impl::gl::ElementBufferId>(
//		ebo, GL_ELEMENT_ARRAY_BUFFER, batch_indices.data(), 0,
//		static_cast<std::uint32_t>(batch_indices.size()), sizeof(impl::Index)
//	);
//
//	// Bind all textures
//	for (std::uint32_t slot = 0; slot < batch_textures.size(); ++slot) {
//		gl_->SetActiveTextureSlot(slot);
//		auto _ = gl_->Bind(batch_textures[slot]);
//	}
//
//	// Draw
//	gl_->DrawElements(
//		vao, static_cast<std::uint32_t>(batch_indices.size()), GL_UNSIGNED_INT, GL_TRIANGLES
//	);
//
//	PTGN_LOG("Draw call");
//
//	// Clear batch (keep white texture)
//	batch_vertices.clear();
//	batch_indices.clear();
//	batch_textures.resize(1);
//	batch_textures[0] = white_texture;
// }
//
// RenderTarget Renderer::AcquirePooledTarget(V2_int size, TextureFormat format) {
//	++pool_tick;
//
//	auto claim = [&](impl::PooledTarget& e) {
//		if (e.target.size != size) {
//			ResizeRenderTarget(e.target, size);
//		}
//		e.in_use		 = true;
//		e.last_used_tick = pool_tick;
//		return e.target;
//	};
//
//	// Find a free candidate:
//	//  - Prefer exact size+format
//	//  - Otherwise pick least-recently-used with same format
//	impl::PooledTarget* exact			= nullptr;
//	impl::PooledTarget* lru_same_format = nullptr;
//
//	for (auto& e : rt_pool) {
//		if (e.in_use) {
//			continue;
//		}
//		if (e.target.format != format) {
//			continue;
//		}
//
//		if (e.target.size == size) {
//			exact = &e;
//			break; // can't beat an exact match
//		}
//
//		if (!lru_same_format || e.last_used_tick < lru_same_format->last_used_tick) {
//			lru_same_format = &e;
//		}
//	}
//
//	if (exact) {
//		return claim(*exact);
//	}
//	if (lru_same_format) {
//		return claim(*lru_same_format);
//	}
//
//	// No compatible free target available.
//	// If we have room in the pool, create one.
//	// Pool is at/over the limit and no compatible spare existed:
//	impl::PooledTarget entry{};
//	entry.target		 = CreateRenderTarget(size, format);
//	entry.in_use		 = true;
//	entry.last_used_tick = pool_tick;
//	rt_pool.push_back(std::move(entry));
//	return rt_pool.back().target;
// }
//
// void Renderer::ReleasePooledTarget(const RenderTarget& target) {
//	++pool_tick;
//
//	for (auto& e : rt_pool) {
//		if (e.target == target) {
//			e.in_use		 = false;
//			e.last_used_tick = pool_tick;
//			return;
//		}
//	}
// }
//
// std::uint32_t Renderer::GetTextureSlot(TextureId tex) {
//	if (tex == white_texture) {
//		return 0; // always slot 0
//	}
//
//	// Check if texture already exists in batch
//	for (std::uint32_t i = 1; i < batch_textures.size(); ++i) {
//		if (batch_textures[i] == tex) {
//			return i;
//		}
//	}
//
//	// Flush if we would exceed GPU texture slots
//	if (batch_textures.size() >= gl_->GetMaxTextureSlots()) {
//		FlushBatch();
//	}
//
//	// Add texture to batch (but do NOT bind yet)
//	batch_textures.push_back(tex);
//
//	// Its slot is index in the vector
//	return static_cast<std::uint32_t>(batch_textures.size() - 1);
// }
//
// template <class State, class Func>
// void UpdateStateIfChanged(Renderer& r, State& cached, const State& desired, Func&& func) {
//	if (!r.state.valid || cached != desired) {
//		r.FlushBatch();
//		cached = desired;
//		std::invoke(std::forward<Func>(func));
//		r.state.valid = true;
//	}
// }
//
// void Renderer::SetShader(impl::gl::ShaderId shader) {
//	UpdateStateIfChanged(*this, state.shader, shader, [this, shader] {
//		auto _ = gl_->Bind(shader);
//	});
// }
//
// void Renderer::SetBlend(BlendMode mode, bool enabled) {
//	impl::gl::BlendState desired{ mode, enabled };
//
//	UpdateStateIfChanged(*this, state.blend, desired, [this, desired] { gl_->SetBlend(desired); });
// }
//
// void Renderer::SetFramebuffer(
//	FramebufferId framebuffer, const impl::gl::Viewport& viewport
//) {
//	UpdateStateIfChanged(*this, state.framebuffer, framebuffer, [this, framebuffer] {
//		auto _ = gl_->Bind(framebuffer);
//	});
//	gl_->SetViewport(viewport);
// }
//
// void Renderer::SetDepth(const impl::gl::DepthState& depth) {
//	UpdateStateIfChanged(*this, state.depth, depth, [this, depth] { gl_->SetDepth(depth); });
// }
//
// void Renderer::SetStencil(const impl::gl::StencilState& stencil) {
//	UpdateStateIfChanged(*this, state.stencil, stencil, [this, stencil] {
//		gl_->SetStencil(stencil);
//	});
// }
//
// void Renderer::SetRaster(const impl::gl::RasterState& raster) {
//	UpdateStateIfChanged(*this, state.raster, raster, [this, raster] { gl_->SetRaster(raster); });
// }
//
// void Renderer::SetColorMask(const impl::gl::ColorMaskState& color_mask) {
//	UpdateStateIfChanged(*this, state.color_mask, color_mask, [this, color_mask] {
//		gl_->SetColorMask(color_mask);
//	});
// }
//
// static std::array<V2_float, 4> MakeQuadPointsPixels(V2_float center, V2_float size) {
//	const V2_float h{ size.x * 0.5f, size.y * 0.5f };
//
//	return { center - h, center + V2_float{ h.x, -h.y }, center + h,
//			 center + V2_float{ -h.x, h.y } };
// }
//
// static constexpr std::array<V2_float, 4> MakeTexCoords(bool flip_y) {
//	if (!flip_y) {
//		return { V2_float{ 0.0f, 0.0f }, V2_float{ 1.0f, 0.0f }, V2_float{ 1.0f, 1.0f },
//				 V2_float{ 0.0f, 1.0f } };
//	} else {
//		return { V2_float{ 0.0f, 1.0f }, V2_float{ 1.0f, 1.0f }, V2_float{ 1.0f, 0.0f },
//				 V2_float{ 0.0f, 0.0f } };
//	}
// }
//
// void Renderer::OnEvent(EventDispatcher d) {
//	d.Dispatch<PresentationResized>([this](auto& e) { PTGN_LOG("Presentation resized: ", e.size); }
//	);
//	d.Dispatch<WindowResized>([this](auto& e) { PTGN_LOG("Window resized: ", e.size); });
//	// TODO: Update physical resolution.
// }
//
// void Renderer::DrawQuadEx(
//	impl::gl::ShaderId shader, const QuadParams& params, const QuadSetup& setup
//) {
//	auto viewport		 = gl_->GetViewport();
//	auto half_viewport	 = viewport.size * 0.5f;
//	auto view_projection = Matrix4::Orthographic(-half_viewport, half_viewport);
//
//	impl::QuadDesc quad{};
//	quad.positions	= MakeQuadPointsPixels(params.center, params.size);
//	quad.tex_coords = params.tex_coords.value_or(MakeTexCoords(params.flip_y));
//	quad.color		= params.tint;
//	quad.rotation	= params.rotation;
//
//	// Texture -> user data slot 0 (convention)
//	if (params.texture) {
//		std::uint32_t slot = GetTextureSlot(*params.texture);
//		quad.user_data[0]  = static_cast<float>(slot);
//	}
//
//	SetShader(shader);
//	gl_->SetUniform(shader, "u_ViewProjection", view_projection);
//
//	setup(shader, quad);
//
//	auto vertices{ impl::Vertex::GetQuad(
//		quad.positions, quad.color, quad.rotation, quad.user_data, quad.tex_coords
//	) };
//
//	constexpr std::array<impl::Index, 6> indices{ 0, 1, 2, 2, 3, 0 };
//
//	if (batch_vertices.size() + vertices.size() >= MaxVertices ||
//		batch_indices.size() + indices.size() >= MaxIndices) {
//		FlushBatch();
//	}
//
//	auto start_index = static_cast<std::uint32_t>(batch_vertices.size());
//
//	batch_vertices.insert(batch_vertices.end(), vertices.begin(), vertices.end());
//
//	for (auto idx : indices) {
//		batch_indices.push_back(idx + start_index);
//	}
// }
//
// void Renderer::DrawQuadEx(
//	impl::gl::ShaderId shader, const QuadParams& params, const UniformSetup& uniforms
//) {
//	DrawQuadEx(shader, params, [uniforms](impl::gl::ShaderId s, impl::QuadDesc&) {
//		if (uniforms) {
//			uniforms(s);
//		}
//	});
// }
//
// void Renderer::DrawTexturedQuad(
//	impl::gl::ShaderId shader, TextureId texture, V2_float center, V2_float size,
//	Color tint, bool flip_y
//) {
//	PTGN_ASSERT(
//		TextureId{
//			gl_->GetFramebufferAttachment(state.framebuffer, GL_COLOR_ATTACHMENT0).id } != texture,
//		"Cannot draw a texture that is attached to the currently set framebuffer"
//	);
//
//	QuadParams p{};
//	p.center  = center;
//	p.size	  = size;
//	p.tint	  = tint;
//	p.texture = texture;
//	p.flip_y  = flip_y;
//
//	DrawQuadEx(shader, p, [this](auto s, auto& q) {
//		gl_->SetUniform(s, "u_Texture", static_cast<std::int32_t>(q.user_data[0]));
//	});
// }
//
// RenderTarget Renderer::CreateRenderTarget(V2_int size, TextureFormat format) const {
//	const auto& desc = impl::gl::GetTextureFormatDesc(format);
//
//	Texture color =
//		gl_->CreateTexture(nullptr, desc.pixel_format, desc.pixel_type, size, desc.internal_format);
//
//	Renderbuffer depth;
//	if (desc.has_depth || desc.has_stencil) {
//		GLenum rb_format = desc.has_stencil ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT;
//
//		depth = gl_->CreateRenderbuffer(size, rb_format);
//	}
//
//	Framebuffer fb = gl_->CreateFramebuffer(
//		color, GL_COLOR_ATTACHMENT0, depth,
//		desc.has_stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT
//	);
//
//	return RenderTarget{
//		.framebuffer = fb, .color = color, .depth = depth, .size = size, .format = format
//	};
// }
//
// void Renderer::ResizeRenderTarget(RenderTarget& rt, V2_int new_size) const {
//	if (rt.size == new_size) {
//		return;
//	}
//
//	gl_->ResizeFramebuffer(rt.framebuffer, new_size);
//
//	rt.size = new_size;
// }
//
// void Renderer::DrawTexture(TextureId texture, V2_float center, V2_float size) {
//	QuadParams p{};
//	p.center  = center;
//	p.size	  = size;
//	p.texture = texture;
//
//	auto shader = gl_->GetShader("quad");
//
//	DrawQuadEx(shader, p);
// }
//
// void Renderer::BeginFrame() {
//	state.valid = false;
//	PTGN_ASSERT(batch_vertices.empty());
//	PTGN_ASSERT(batch_indices.empty());
//
//	auto _1 = gl_->Bind(FramebufferId{});
//	gl_->SetClearColor(color::Transparent);
//	gl_->Clear();
//
//	BindRenderTarget(screen_target);
//	gl_->ClearToColor(screen_target.framebuffer, color::Transparent);
// }
//
// void Renderer::EndFrame() {
//	auto window_size = window_.GetSize();
//
//	SetFramebuffer({}, { { 0, 0 }, window_size });
//	SetBlend(BlendMode::ReplaceRGBA);
//
//	DrawTexture(screen_target.color, { 0, 0 }, screen_target.size);
//
//	FlushBatch();
// }
//
// void Renderer::BindRenderTarget(
//	FramebufferId framebuffer, const impl::gl::Viewport& viewport
//) {
//	SetFramebuffer(framebuffer, viewport);
// }
//
// void Renderer::BindRenderTarget(const RenderTarget& rt) {
//	BindRenderTarget(rt.framebuffer, { { 0, 0 }, rt.size });
// }
//
//
// void Renderer::DrawLightQuad(const LightParams& light) {
//	QuadParams p{};
//	p.center = light.position;
//	p.size	 = { light.radius * 2.0f, light.radius * 2.0f };
//	p.tint	 = color::White;
//
//	auto shader = gl_->GetShader("light");
//
//	DrawQuadEx(shader, p, [&](const auto& s, auto&) {
//		gl_->SetUniform(s, "u_LightPosition", light.position);
//		gl_->SetUniform(s, "u_Color", light.color.Normalized());
//		gl_->SetUniform(s, "u_LightIntensity", light.intensity);
//		gl_->SetUniform(s, "u_LightRadius", light.radius);
//		gl_->SetUniform(s, "u_Falloff", light.falloff);
//		gl_->SetUniform(s, "u_AmbientColor", light.ambient_color);
//		gl_->SetUniform(s, "u_AmbientIntensity", light.ambient_intensity);
//		gl_->SetUniform(s, "u_LightAttenuation", light.attenuation);
//	});
// }
//*/
//
// } // namespace ptgn
//
//
//
// static float GetFade(float diameter_y) {
//	constexpr float fade_scaling_constant{ 0.12f };
//	return fade_scaling_constant / diameter_y;
// }
//
// static float GetFade(V2_float diameter) {
//	return GetFade(diameter.y);
// }
//
// static float NormalizeArcLineWidthToThickness(float line_width, float fade, V2_float radii) {
//	if (line_width == -1.0f) {
//		// Internally line width for a filled SDF is 1.0f.
//		line_width = 1.0f;
//	} else {
//		PTGN_ASSERT(line_width >= min_line_width, "Invalid line width for circle");
//
//		// Internally line width for a completely hollow ellipse is 0.0f.
//		line_width = fade + line_width / std::min(radii.x, radii.y);
//	}
//	return line_width;
// }
//
// static float GetAspectRatio(V2_float size) {
//	PTGN_ASSERT(size.x > 0.0f);
//	return size.y / size.x;
// }
//
// static float GetNormalizedRadius(float diameter, float size_x) {
//	PTGN_ASSERT(size_x > 0.0f);
//	float normalized_radius{ diameter / size_x };
//	return std::clamp(normalized_radius, 0.0f, 1.0f);
// }
//
// template <ShapeType T>
// static std::array<float, 4> GetData(
//	const T& shape, auto radius, float line_width, V2_float size
//) {
//	std::array<float, 4> data{ 0.0f, 0.0f, 0.0f, 0.0f };
//
//	auto diameter{ 2.0f * radius };
//
//	float fade{ GetFade(diameter) };
//
//	float thickness{ NormalizeArcLineWidthToThickness(line_width, fade, V2_float{ radius }) };
//
//	data[0] = thickness;
//	data[1] = fade;
//
//	if constexpr (std::is_same_v<T, Arc>) {
//		float aperture{ shape.GetAperture() };
//		float direction{ shape.clockwise ? 1.0f : -1.0f };
//
//		data[2] = aperture;
//		data[3] = direction;
//	} else if constexpr (IsAnyOf<T, Capsule, RoundedRect>) {
//		float normalized_radius{ GetNormalizedRadius(diameter, size.x) };
//		float aspect_ratio{ GetAspectRatio(size) };
//
//		data[2] = normalized_radius;
//		data[3] = aspect_ratio;
//	}
//
//	return data;
// }
//
// struct QuadInfo {
//	std::array<V2_float, 4> points;
//	std::array<float, 4> data{ 0.0f, 0.0f, 0.0f, 0.0f };
// };
//
// template <ShapeType T>
// static std::optional<QuadInfo> GetQuadInfo(Renderer& ctx, DrawShapeCommand& cmd, const T& shape)
// { 	QuadInfo info;
//
//	const auto set_shader = [](DrawShapeCommand& c, std::string_view shader_name) {
//		if (c.render_state.shader_pass.has_value() && *c.render_state.shader_pass != ShaderPass{}) {
//			return;
//		}
//		c.render_state.shader_pass = Application::Get().shader.Get(shader_name);
//	};
//
//	if constexpr (std::is_same_v<T, V2_float>) {
//		Transform translated = cmd.transform;
//		translated.Translate(shape);
//
//		Rect r{ V2_float{ 1.0f } };
//
//		info.points = r.GetWorldVertices(translated, Origin::Center);
//	} else if constexpr (std::is_same_v<T, Line>) {
//		if (cmd.line_width < min_line_width) {
//			return std::nullopt;
//		}
//
//		info.points = shape.GetWorldQuadVertices(cmd.transform, cmd.line_width);
//	} else if constexpr (std::is_same_v<T, Capsule>) {
//		auto radius{ shape.GetRadius(cmd.transform) };
//
//		if (radius <= 0.0f) {
//			return std::nullopt;
//		}
//
//		V2_float size;
//
//		info.points = shape.GetWorldQuadVertices(cmd.transform, &size);
//		info.data	= GetData(shape, radius, cmd.line_width, size);
//
//		set_shader(cmd, "capsule");
//	} else if constexpr (std::is_same_v<T, Arc>) {
//		auto radius{ shape.GetRadius(cmd.transform) };
//
//		if (radius <= 0.0f) {
//			return std::nullopt;
//		}
//
//		Transform rotated{ cmd.transform };
//		rotated.Rotate(shape.GetStartAngle());
//
//		info.points = shape.GetWorldQuadVertices(rotated);
//		info.data	= GetData(shape, radius, cmd.line_width, {});
//
//		set_shader(cmd, "arc");
//	} else if constexpr (std::is_same_v<T, RoundedRect>) {
//		auto size = shape.GetSize(cmd.transform);
//
//		if (!size.BothAboveZero()) {
//			return std::nullopt;
//		}
//
//		float radius = shape.GetRadius(cmd.transform);
//
//		if (radius <= 0.0f) {
//			cmd.render_state.shader_pass = std::nullopt;
//			cmd.shape					 = Rect{ shape.GetSize() };
//			ctx.DrawCommand(cmd);
//			return std::nullopt;
//		}
//
//		info.points = shape.GetWorldQuadVertices(cmd.transform, cmd.origin);
//		info.data	= GetData(shape, radius, cmd.line_width, size);
//
//		set_shader(cmd, "rounded_rect");
//	} else if constexpr (std::is_same_v<T, Ellipse>) {
//		auto radius = shape.GetRadius(cmd.transform);
//
//		if (!radius.BothAboveZero()) {
//			return std::nullopt;
//		}
//
//		info.points = shape.GetWorldQuadVertices(cmd.transform);
//		info.data	= GetData(shape, radius, cmd.line_width, {});
//
//		set_shader(cmd, "circle");
//	} else {
//		return std::nullopt;
//	}
//
//	return info;
// }
//
// template <ShapeType T>
// static void DrawShape(Renderer& ctx, DrawShapeCommand cmd, const T& shape) {
//	if constexpr (IsAnyOf<T, V2_float, Line, Capsule, Arc, RoundedRect, Ellipse>) {
//		auto info{ GetQuadInfo(ctx, cmd, shape) };
//
//		if (!info.has_value()) {
//			return;
//		}
//
//		const auto& [points, data] = *info;
//
//		auto quad_vertices{
//			Vertex::GetQuad(points, cmd.tint, cmd.depth, data, GetDefaultTextureCoordinates())
//		};
//
//		ctx.SetState(cmd.render_state);
//		ctx.AddVertices(quad_vertices, quad_indices);
//	} else if constexpr (std::is_same_v<T, Circle>) {
//		cmd.shape = Ellipse{ V2_float{ shape.GetRadius() } };
//		ctx.DrawCommand(cmd);
//	} else if constexpr (std::is_same_v<T, Rect>) {
//		if (auto size{ shape.GetSize(cmd.transform) }; !size.BothAboveZero()) {
//			return;
//		}
//
//		auto points = shape.GetWorldVertices(cmd.transform, cmd.origin);
//		auto vertices =
//			Vertex::GetQuad(points, cmd.tint, cmd.depth, { 0.0f }, GetDefaultTextureCoordinates());
//
//		ctx.SetState(cmd.render_state);
//
//		if (cmd.line_width == -1.0f) {
//			ctx.AddVertices(vertices, quad_indices);
//		} else {
//			ctx.AddLinesImpl(vertices, quad_indices, points, cmd.line_width, {});
//		}
//
//	} else if constexpr (std::is_same_v<T, Triangle>) {
//		auto points	  = shape.GetWorldVertices(cmd.transform);
//		auto vertices = Vertex::GetTriangle(points, cmd.tint, cmd.depth);
//
//		ctx.SetState(cmd.render_state);
//
//		if (cmd.line_width == -1.0f) {
//			ctx.AddVertices(vertices, triangle_indices);
//		} else {
//			ctx.AddLinesImpl(vertices, triangle_indices, points, cmd.line_width, {});
//		}
//	} else if constexpr (std::is_same_v<T, Polygon>) {
//		ctx.SetState(cmd.render_state);
//
//		if (shape.vertices.size() < 3) {
//			if (shape.vertices.empty()) {
//				return;
//			} else if (shape.vertices.size() == 1) {
//				cmd.shape = V2_float{ shape.vertices.front() };
//				ctx.DrawCommand(cmd);
//				return;
//			} else if (shape.vertices.size() == 2) {
//				cmd.shape = Line{ shape.vertices[0], shape.vertices[1] };
//				ctx.DrawCommand(cmd);
//				return;
//			}
//		}
//
//		auto points = shape.GetWorldVertices(cmd.transform);
//
//		if (cmd.line_width == -1.0f) {
//			auto triangles{ Triangulate(points) };
//			for (const auto& triangle : triangles) {
//				auto vertices = Vertex::GetTriangle(triangle, cmd.tint, cmd.depth);
//				ctx.AddVertices(vertices, triangle_indices);
//			}
//		} else {
//			auto vertices =
//				Vertex::GetQuad({}, cmd.tint, cmd.depth, { 0.0f }, GetDefaultTextureCoordinates());
//			ctx.AddLinesImpl(vertices, quad_indices, points, cmd.line_width, {});
//		}
//	}
// }
//
// void Renderer::DrawLines(const DrawLinesCommand& cmd) {
//	std::size_t count = cmd.points.size();
//
//	PTGN_ASSERT(cmd.line_width >= min_line_width);
//
//	PTGN_ASSERT(
//		(cmd.connect_last_to_first && count >= 3) || (!cmd.connect_last_to_first && count >= 2)
//	);
//
//	std::size_t vertex_modulo = count;
//	if (!cmd.connect_last_to_first) {
//		vertex_modulo -= 1;
//	}
//
//	SetState(cmd.render_state);
//
//	for (std::size_t i = 0; i < count; ++i) {
//		Line l{ cmd.points[i], cmd.points[(i + 1) % vertex_modulo] };
//		auto quad_points   = l.GetWorldQuadVertices(cmd.transform, cmd.line_width);
//		auto quad_vertices = Vertex::GetQuad(
//			quad_points, cmd.tint, cmd.depth, { 0.0f }, GetDefaultTextureCoordinates()
//		);
//		AddVertices(quad_vertices, quad_indices);
//	}
// }
//
// void Renderer::AddTemporaryTexture(Texture&& texture) {
//	temporary_textures.emplace_back(std::move(texture));
// }
//
// void Renderer::AddLinesImpl(
//	std::span<Vertex> line_vertices, std::span<const Index> line_indices,
//	std::span<const V2_float> points, float line_width, const Transform& transform
//) {
//	PTGN_ASSERT(line_width >= min_line_width, "Invalid line width for lines");
//
//	for (std::size_t i = 0; i < points.size(); ++i) {
//		Line l{ points[i], points[(i + 1) % points.size()] };
//		auto line_points{ l.GetWorldQuadVertices(transform, line_width) };
//
//		PTGN_ASSERT(line_vertices.size() <= line_points.size());
//
//		for (std::size_t j = 0; j < line_vertices.size(); ++j) {
//			line_vertices[j].position[0] = line_points[j].x;
//			line_vertices[j].position[1] = line_points[j].y;
//		}
//
//		AddVertices(line_vertices, line_indices);
//	}
// }
//
// void Renderer::Flush(bool final_flush) {
//	std::vector<TextureId> texture_id;
//
//	bool has_post_fx{ !render_state.post_fx.post_fx_.empty() };
//
//	auto target{ drawing_to_ };
//
//	if (render_state.IsSet()) {
//		if (render_state.camera) {
//			target.view_projection = render_state.camera;
//			target.points		   = render_state.camera.GetWorldVertices();
//		}
//		target.blend_mode = render_state.blend_mode;
//	}
//
//	if (has_post_fx) {
//		PTGN_ASSERT(!intermediate_target);
//
//		intermediate_target = draw_context_pool.Get(target.viewport.size, target.texture_format);
//
//		target.framebuffer = &intermediate_target->framebuffer;
//
//		const auto& shader{ GetCurrentShader() };
//
//		// Draw unflushed vertices to intermediate target before adding post fx to it.
//		DrawCall(
//			shader, vertices_, indices_, textures_, target.framebuffer, true, color::Transparent,
//			target.blend_mode, target.viewport, target.view_projection
//		);
//
//		// Add post fx to the intermediate target.
//
//		// Flip only every odd ping pong to keep the flushed target upright.
//		bool flip{ render_state.post_fx.post_fx_.size() % 2 == 1 };
//		auto id{ PingPong(render_state.post_fx.post_fx_, intermediate_target, {}, target, flip) };
//		target.texture_id = id;
//	}
//
//	// Reset because post fx may change target.framebuffer.
//	target.framebuffer = drawing_to_.framebuffer;
//
//	if (intermediate_target) {
//		// This branch is for when an intermediate target needs to be flushed onto the
//		// drawing_to frame buffer. It is used in cases where postfx are applied, or when a
//		// shader that uses the intermediate target is being flushed (for instance a set of
//		// lights rendered onto an intermediate target and then flushed onto the drawing_to
//		// frame buffer).
//
//		if (!has_post_fx) {
//			// The light case discussed above.
//			const auto& texture{ intermediate_target->framebuffer.GetTexture() };
//			target.texture_id	  = texture.GetId();
//			target.texture_format = texture.GetFormat();
//			target.texture_size	  = texture.GetSize();
//		}
//		if (intermediate_target->blend_mode.has_value()) {
//			target.blend_mode = *intermediate_target->blend_mode;
//		}
//
//		// Only flip if postfx have been applied.
//		DrawCall(
//			GetFullscreenShader(target.texture_format),
//			Vertex::GetQuad(
//				target.points, target.tint, target.depth, { 1.0f }, GetDefaultTextureCoordinates(),
//				has_post_fx
//			),
//			quad_indices, { target.texture_id }, target.framebuffer, false, color::Transparent,
//			target.blend_mode, target.viewport, target.view_projection
//		);
//
//	} else if (render_state.IsSet()) {
//		// No post fx, and no intermediate target.
//
//		const auto& shader{ GetCurrentShader() };
//
//		// Draw unflushed vertices directly to drawing_to frame buffer.
//		DrawCall(
//			shader, vertices_, indices_, textures_, target.framebuffer, false, color::Transparent,
//			target.blend_mode, target.viewport, target.view_projection
//		);
//	}
//
//	Reset();
//
//	if (final_flush) {
//		render_state = {};
//	}
// }
//
// void Renderer::Reset() {
//	intermediate_target = {};
//	vertices_.clear();
//	indices_.clear();
//	textures_.clear();
//	index_offset_ = 0;
//	force_flush	  = false;
//	draw_context_pool.TrimExpired();
// }
//
//
// void Renderer::SetDrawingTo(const RenderTarget& render_target) {
//	const auto& texture{ render_target.GetTexture() };
//	auto texture_size{ render_target.GetTextureSize() };
//	Camera camera{ render_target.GetCamera() };
//
//	drawing_to_.texture_size	  = texture_size;
//	drawing_to_.texture_id		  = texture.GetId();
//	drawing_to_.texture_format	  = texture.GetFormat();
//	drawing_to_.viewport.position = {};
//	drawing_to_.viewport.size	  = texture_size;
//
//	drawing_to_.view_projection = camera;
//	drawing_to_.points			= camera.GetWorldVertices();
//
//	drawing_to_.blend_mode	 = GetBlendMode(render_target);
//	drawing_to_.depth		 = GetDepth(render_target);
//	drawing_to_.tint		 = GetTint(render_target);
//	drawing_to_.framebuffer = &render_target.GetFramebuffer();
// }
//
// void Renderer::DrawScreenTarget() {
//	auto half_viewport{ display_viewport_.size * 0.5f };
//
//	const auto& texture{ screen_target_.GetTexture() };
//
//	DrawCall(
//		GetFullscreenShader(texture.GetFormat()),
//		Vertex::GetQuad(
//			{ -half_viewport, V2_float{ half_viewport.x, -half_viewport.y }, half_viewport,
//			  V2_float{ -half_viewport.x, half_viewport.y } },
//			GetTint(screen_target_), GetDepth(screen_target_), { 1.0f },
//			GetDefaultTextureCoordinates(), true
//		),
//		quad_indices, { texture.GetId() }, nullptr, false, color::Transparent,
//		GetBlendMode(screen_target_), display_viewport_,
//		Matrix4::Orthographic(-half_viewport, half_viewport)
//	);
// }
//
// void Renderer::Draw(Scene& scene) {
//	// PTGN_LOG(draw_context_pool.contexts_.size());
//	// PTGN_PROFILE_FUNCTION();
//
//	white_texture.Bind(0);
//
//	DrawScene(scene);
//
//	auto half_game_size{ game_size_ * 0.5f };
//
//	Transform scene_transform{ GetTransform(scene.render_target_) };
//
//	auto points{ Rect{ scene.camera.GetViewportSize() }.GetWorldVertices(scene_transform) };
//	auto projection{ Matrix4::Orthographic(-half_game_size, half_game_size) };
//
//	Viewport viewport{ {}, display_viewport_.size };
//
//	const auto& texture{ scene.render_target_.GetTexture() };
//
//	DrawCall(
//		GetFullscreenShader(texture.GetFormat()),
//		Vertex::GetQuad(
//			points, GetTint(scene.render_target_), GetDepth(scene.render_target_), { 1.0f },
//			GetDefaultTextureCoordinates(), true
//		),
//		quad_indices, { texture.GetId() }, &screen_target_.GetFramebuffer(), false,
//		color::Transparent, GetBlendMode(scene.render_target_), viewport, projection
//	);
//
//	draw_queues_.clear();
//	debug_queue_.clear();
//
//	Reset();
//
//	render_state	   = {};
//	temporary_textures = std::vector<Texture>{};
// }
//
// } // namespace impl
//
//// TODO: Get rid of this.
// using namespace impl;
//
// void Renderer::DrawTexture(
//	const Texture& texture, const Transform& transform, V2_float texture_size, Origin origin,
//	const Tint& tint, const Depth& depth, const std::array<V2_float, 4>& texture_coordinates
//) {
//	Rect rect{ !texture_size.IsZero() ? texture_size : V2_float{ texture.GetSize() } };
//
//	DrawTextureCommand cmd;
//
//	cmd.transform				= transform;
//	cmd.texture_id				= texture.GetId();
//	cmd.texture_size			= texture.GetSize();
//	cmd.texture_format			= texture.GetFormat();
//	cmd.rect					= rect;
//	cmd.origin					= origin;
//	cmd.depth					= depth;
//	cmd.pre_fx					= pre_fx;
//	cmd.tint					= tint;
//	cmd.texture_coordinates		= texture_coordinates;
//	cmd.render_state.blend_mode = blend_mode;
//	cmd.render_state.camera		= camera;
//	cmd.render_state.post_fx	= post_fx;
//
//	render_data_.Submit(cmd);
// }
//
// void Renderer::DrawLines(
//	const Transform& transform, const std::vector<V2_float>& line_points, const Tint& color,
//	const LineWidth& line_width, bool connect_last_to_first, const Depth& depth
//) {
//	DrawLinesCommand cmd;
//
//	cmd.transform				= transform;
//	cmd.points					= line_points;
//	cmd.tint					= color;
//	cmd.line_width				= line_width;
//	cmd.connect_last_to_first	= connect_last_to_first;
//	cmd.depth					= depth;
//	cmd.render_state.blend_mode = blend_mode;
//	cmd.render_state.camera		= camera;
//	cmd.render_state.post_fx	= post_fx;
//
//	render_data_.Submit(cmd);
// }
//
// void Renderer::DrawLines(
//	const std::vector<V2_float>& line_points, const Tint& color, const LineWidth& line_width,
//	bool connect_last_to_first, const Depth& depth
//) {
//	DrawLines(
//		{}, line_points, color, line_width, connect_last_to_first, depth, blend_mode, camera,
//		post_fx
//	);
// }
//
// void Renderer::DrawShape(
//	const Transform& transform, const Shape& shape, const Tint& color, const LineWidth& line_width,
//	Origin origin, const Depth& depth, const ShaderPass& shader_pass
//) {
//	DrawShapeCommand cmd;
//
//	cmd.transform				 = transform;
//	cmd.shape					 = shape;
//	cmd.tint					 = color;
//	cmd.line_width				 = line_width;
//	cmd.origin					 = origin;
//	cmd.depth					 = depth;
//	cmd.render_state.shader_pass = shader_pass;
//	cmd.render_state.blend_mode	 = blend_mode;
//	cmd.render_state.camera		 = camera;
//	cmd.render_state.post_fx	 = post_fx;
//
//	render_data_.Submit(cmd);
// }
//
// void Renderer::DrawShader(
//	const ShaderPass& shader_pass, const Entity& entity, bool clear_between_consecutive_calls,
//	Color target_clear_color, const TextureOrSize& texture_or_size,const Depth& depth
//) {
//	DrawShaderCommand cmd;
//
//	cmd.entity							= entity;
//	cmd.clear_between_consecutive_calls = clear_between_consecutive_calls;
//	cmd.target_clear_color				= target_clear_color;
//	cmd.texture_or_size					= texture_or_size;
//	cmd.intermediate_blend_mode			= intermediate_blend_mode;
//	cmd.target_blend_mode				= target_blend_mode;
//	cmd.depth							= depth;
//	cmd.texture_format					= texture_format;
//	cmd.render_state.shader_pass		= shader_pass;
//	cmd.render_state.post_fx			= post_fx;
//	cmd.render_state.blend_mode			= blend_mode;
//	cmd.render_state.camera				= camera;
//
//	render_data_.Submit(cmd);
// }
//
// impl::Texture Renderer::CreateTexture(
//	Transform& out_transform, V2_float& out_text_size, const TextContent& content,
//	const TextColor& color, const FontSize& font_size, const ResourceHandle& font_key,
//	const TextProperties& properties, bool hd_text
//) {
//	FontSize final_font_size{ font_size };
//
//	if (hd_text) {
//		// TODO: Figure out a better solution to this.
//		const auto& scene{ ctx_->scene->GetCurrent() };
//
//		auto render_target_scale{ scene->GetRenderTargetScaleRelativeTo(camera) };
//
//		PTGN_ASSERT(render_target_scale.BothAboveZero());
//
//		out_transform.Scale(1.0f / render_target_scale);
//
//		final_font_size =
//			static_cast<std::int32_t>(static_cast<float>(font_size) * render_target_scale.y);
//	}
//
//	auto texture{ Text::CreateTexture(content, color, final_font_size, font_key, properties) };
//
//	if (out_text_size.IsZero()) {
//		out_text_size = Text::GetSize(content, font_key, final_font_size);
//	}
//
//	return texture;
// }
//
// void Renderer::DrawText(
//	const std::string& content, Transform transform, const TextColor& color, Origin origin,
//	const FontSize& font_size, const ResourceHandle& font_key, const TextProperties& properties,
//	V2_float text_size, const Tint& tint, bool hd_text, const Depth& depth
//	const std::array<V2_float, 4>& texture_coordinates
//) {
//	auto texture{ CreateTexture(
//		transform, text_size, content, color, font_size, font_key, properties, hd_text, camera
//	) };
//
//	DrawTexture(
//		texture, transform, text_size, origin, tint, depth, blend_mode, camera, pre_fx, post_fx,
//		texture_coordinates
//	);
//
//	render_data_.AddTemporaryTexture(std::move(texture));
// }
//
// void Renderer::DrawRect(
//	const Transform& transform, const Rect& rect, const Tint& color, const LineWidth& line_width,
//	Origin origin, const Depth& depth
//) {
//	DrawShape(transform, rect, color, line_width, origin, depth, blend_mode, camera, post_fx);
// }
//
// void Renderer::DrawRoundedRect(
//	const Transform& transform, const RoundedRect& rounded_rect, const Tint& color,
//	const LineWidth& line_width, Origin origin, const Depth& depth
//) {
//	DrawShape(
//		transform, rounded_rect, color, line_width, origin, depth, blend_mode, camera, post_fx
//	);
// }
//
// void Renderer::DrawLine(
//	V2_float start, V2_float end, const Tint& color, const LineWidth& line_width,
//	const Depth& depth
//) {
//	DrawLine({}, Line{ start, end }, color, line_width, depth, blend_mode, camera, post_fx);
// }
//
// void Renderer::DrawLine(
//	const Transform& transform, const Line& line, const Tint& color, const LineWidth& line_width,
//	const Depth& depth
//) {
//	DrawShape(
//		transform, line, color, line_width, Origin::Center, depth, blend_mode, camera, post_fx
//	);
// }
//
// void Renderer::DrawTriangle(
//	const Transform& transform, const Triangle& triangle, const Tint& color,
//	const LineWidth& line_width, const Depth& depth, BlendMode blend_mode, const Camera& camera,
//	const PostFX& post_fx
//) {
//	DrawShape(
//		transform, triangle, color, line_width, Origin::Center, depth, blend_mode, camera, post_fx
//	);
// }
//
// void Renderer::DrawEllipse(
//	const Transform& transform, const Ellipse& ellipse, const Tint& color,
//	const LineWidth& line_width, const Depth& depth, BlendMode blend_mode, const Camera& camera,
//	const PostFX& post_fx
//) {
//	DrawShape(
//		transform, ellipse, color, line_width, Origin::Center, depth, blend_mode, camera, post_fx
//	);
// }
//
// void Renderer::DrawCircle(
//	const Transform& transform, const Circle& circle, const Tint& color,
//	const LineWidth& line_width, const Depth& depth, BlendMode blend_mode, const Camera& camera,
//	const PostFX& post_fx
//) {
//	DrawShape(
//		transform, circle, color, line_width, Origin::Center, depth, blend_mode, camera, post_fx
//	);
// }
//
// void Renderer::DrawCapsule(
//	const Transform& transform, const Capsule& capsule, const Tint& color,
//	const LineWidth& line_width, const Depth& depth, BlendMode blend_mode, const Camera& camera,
//	const PostFX& post_fx
//) {
//	DrawShape(
//		transform, capsule, color, line_width, Origin::Center, depth, blend_mode, camera, post_fx
//	);
// }
//
// void Renderer::DrawArc(
//	const Transform& transform, const Arc& arc, const Tint& color, const LineWidth& line_width,
//	const Depth& depth
//) {
//	DrawShape(
//		transform, arc, color, line_width, Origin::Center, depth, blend_mode, camera, post_fx
//	);
// }
//
// void Renderer::DrawPolygon(
//	const Transform& transform, const Polygon& polygon, const Tint& color,
//	const LineWidth& line_width, const Depth& depth, BlendMode blend_mode, const Camera& camera,
//	const PostFX& post_fx
//) {
//	DrawShape(
//		transform, polygon, color, line_width, Origin::Center, depth, blend_mode, camera, post_fx
//	);
// }
//
// void Renderer::DrawPoint(
//	V2_float point, const Tint& color, const Depth& depth, BlendMode blend_mode,
//	const Camera& camera
//) {
//	DrawShape({}, point, color, -1.0f, Origin::Center, depth, blend_mode, camera, {});
// }
//
// void Renderer::PresentScreen() {
//	Framebuffer::Unbind();
//
//	// PTGN_ASSERT(
//	// 	std::invoke([]() {
//	// 		auto viewport_size{ GLRenderer::GetViewportSize() };
//	// 		if (viewport_size.IsZero()) {
//	// 			return false;
//	// 		}
//	// 		if (viewport_size.x == 1 && viewport_size.y == 1) {
//	// 			return false;
//	// 		}
//	// 		return true;
//	// 	}),
//	// 	"Attempting to render to 0 or 1 sized viewport"
//	// );
//
//	PTGN_ASSERT(
//		Framebuffer::IsUnbound(),
//		"Frame buffer must be unbound (id=0) before swapping SDL buffer to the screen"
//	);
//
//	window->SwapBuffers();
// }
//
//*/