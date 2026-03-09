
#include "renderer/renderer.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string_view>

#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/log.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "platform/input/events.h"
#include "platform/window/window.h"
#include "renderer/backend/gl/gl_renderer.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/render_state.h"
#include "renderer/primitives/render_target.h"
#include "renderer/primitives/scaling_mode.h"
#include "renderer/primitives/shader.h"
#include "renderer/primitives/texture.h"
#include "renderer/primitives/viewport.h"
#include "runtime/event/event_handler.h"

namespace ptgn {

RenderContext::RenderContext(Renderer& renderer) : renderer_{ renderer } {}

void RenderContext::DrawLine(
	impl::ShaderId shader, const std::array<V2_float, 2>& positions, Color tint, float depth
) {
	renderer_.gl_renderer_->DrawLine(shader, positions, tint, depth);
}

void RenderContext::DrawTriangle(
	impl::ShaderId shader, const std::array<V2_float, 3>& positions, Color tint, float depth
) {
	renderer_.gl_renderer_->DrawTriangle(shader, positions, tint, depth);
}

void RenderContext::DrawQuad(
	impl::ShaderId shader, const std::array<V2_float, 4>& positions,
	const std::array<float, 4>& user_data, Color tint, float depth
) {
	renderer_.gl_renderer_->DrawQuad(shader, positions, user_data, tint, depth);
}

void RenderContext::DrawTexture(
	impl::ShaderId shader, impl::TextureId texture, const std::array<V2_float, 4>& positions,
	Color tint, float depth, bool flip_y, const std::optional<std::array<V2_float, 4>>& tex_coords
) {
	renderer_.gl_renderer_->DrawTexture(
		shader, texture, positions, tint, depth, flip_y, tex_coords
	);
}

void RenderContext::DrawTexture(
	impl::TextureId texture, const std::array<V2_float, 4>& positions, Color tint, float depth,
	bool flip_y, const std::optional<std::array<V2_float, 4>>& tex_coords
) {
	auto quad_shader{ GetShader("quad") };
	DrawTexture(quad_shader, texture, positions, tint, depth, flip_y, tex_coords);
}

void RenderContext::DrawQuad(const std::array<V2_float, 4>& positions, Color tint, float depth) {
	auto white_texture{ GetWhiteTexture() };
	DrawTexture(white_texture, positions, tint, depth, false, {});
}

void RenderContext::BindScreenTarget() {
	return renderer_.gl_renderer_->BindScreenTarget();
}

void RenderContext::SetViewport(Viewport viewport) {
	renderer_.gl_renderer_->SetViewport(viewport);
}

void RenderContext::SetViewProjection(const Matrix4& view_projection) {
	renderer_.gl_renderer_->SetViewProjection(view_projection);
}

void RenderContext::SetBlend(BlendMode mode, bool enabled) {
	renderer_.gl_renderer_->SetBlend(mode, enabled);
}

void RenderContext::SetDepth(const DepthState& depth) {
	renderer_.gl_renderer_->SetDepth(depth);
}

void RenderContext::SetStencil(const StencilState& stencil) {
	renderer_.gl_renderer_->SetStencil(stencil);
}

void RenderContext::SetRaster(const RasterState& raster) {
	renderer_.gl_renderer_->SetRaster(raster);
}

void RenderContext::SetScissor(const ScissorState& scissor) {
	renderer_.gl_renderer_->SetScissor(scissor);
}

void RenderContext::SetColorMask(const ColorMaskState& color_mask) {
	renderer_.gl_renderer_->SetColorMask(color_mask);
}

impl::RenderPass RenderContext::BeginPass(const impl::RenderTargetData& scene_target) {
	return renderer_.gl_renderer_->BeginPass(scene_target);
}

impl::TextureId RenderContext::GetWhiteTexture() const {
	return renderer_.gl_renderer_->GetWhiteTexture();
}

impl::ShaderId RenderContext::GetShader(std::string_view name) const {
	return renderer_.gl_renderer_->GetShader(name);
}

Renderer::Renderer(Window& window, EventHandler& events) :
	window_{ window },
	events_{ events },
	gl_renderer_{ std::make_shared<impl::gl::GLRenderer>(window) },
	context_{ *this } {}

Renderer::~Renderer() noexcept {
	// Destructor access to impl::gl::GLRenderer is needed.
}

RenderContext& Renderer::GetContext() {
	return context_;
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

	PTGN_ASSERT(
		!game_size.has_value() || game_size.has_value() && game_size->BothAboveZero(),
		"Game size cannot be negative or zero"
	);

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

impl::RenderTargetObject Renderer::CreateRenderTarget(V2_int size, TextureFormat format) {
	return gl_renderer_->CreateRenderTarget(size, format);
}

void Renderer::SetBackgroundColor(Color background_color) {
	gl_renderer_->SetBackgroundColor(background_color);
}

[[nodiscard]] Color Renderer::GetBackgroundColor() const {
	return gl_renderer_->GetBackgroundColor();
}

Viewport Renderer::GetDisplayViewport() const {
	return display_viewport_;
}

void Renderer::BeginFrame() {
	gl_renderer_->BeginFrame(window_.GetSize(), window_.GetBackgroundColor());
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
			gl_renderer_->ResizeScreenTarget(display_viewport_.size);

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