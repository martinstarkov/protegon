#include "renderer/renderer.h"

#include <type_traits>

#include "core/game.h"
#include "core/window.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/gl_renderer.h"
#include "renderer/render_data.h"
#include "scene/scene_manager.h"
#include "utility/assert.h"
#include "utility/debug.h"
#include "utility/log.h"
#include "utility/stats.h"

namespace ptgn::impl {

void Renderer::Init(const Color& window_background_color) {
	background_color_ = window_background_color;
	ClearScreen();
	// GLRenderer::EnableLineSmoothing();
	// TODO: Fix.
	/*screen_target_ = RenderTarget{ window_background_color, BlendMode::Blend };
	current_target_ = screen_target_;*/

	render_data_.Init();
}

void Renderer::Reset() {
	resolution_	  = {};
	scaling_mode_ = ResolutionMode::Disabled;

	// TODO: Fix.
	/*current_target_ = {};
	screen_target_	= {};*/

	bound_ = {};

	FrameBuffer::Unbind(); // Will set bound_frame_buffer_id_ to 0.
}

void Renderer::Shutdown() {
	Reset();
}

// TODO: Fix.
/*
void Renderer::SetRenderTarget(const RenderTarget& target) {
	if (current_target_ == target) {
		return;
	}
	Flush();
	current_target_ = target.IsValid() ? target : screen_target_;
}

RenderTarget Renderer::GetRenderTarget() const {
	return current_target_;
}

void Renderer::Clear() const {
	current_target_.Get().Clear();
}

void Renderer::Flush() {
	current_target_.Get().Bind();

	GLRenderer::SetBlendMode(current_target_.Get().GetBlendMode());
	GLRenderer::SetViewport(
		current_target_.Get().viewport_.Min(), current_target_.Get().viewport_.size
	);

	if (render_data_.SetViewProjection(current_target_.Get().GetCamera())) {
		// Post mouse event when camera projection changes.
		game.event.mouse.Post(MouseEvent::Move, MouseMoveEvent{});
	}

	render_data_.Flush();

	PTGN_ASSERT(
		current_target_.Get().frame_buffer_.IsBound(),
		"Flushing the renderer must leave the current render target bound"
	);
}

BlendMode Renderer::GetBlendMode() const {
	return current_target_.Get().GetBlendMode();
}

void Renderer::SetBlendMode(BlendMode blend_mode) {
	if (current_target_.Get().GetBlendMode() == blend_mode) {
		return;
	}
	Flush();
	current_target_.Get().SetBlendMode(blend_mode);
}

Color Renderer::GetClearColor() const {
	return current_target_.Get().GetClearColor();
}

void Renderer::SetClearColor(const Color& clear_color) {
	current_target_.Get().SetClearColor(clear_color);
}

void Renderer::SetViewport(const Rect& viewport) {
	current_target_.Get().SetViewport(viewport);
}

Rect Renderer::GetViewport() const {
	return current_target_.Get().GetViewport();
}

void Renderer::SetResolution(const V2_int& resolution) {
	resolution_ = resolution;
	// User expects setting resolution to take effect immediately so it is defaulted to stretch.
	if (scaling_mode_ == ResolutionMode::Disabled) {
		scaling_mode_ = ResolutionMode::Stretch;
	}
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

*/

RenderData& Renderer::GetRenderData() {
	return render_data_;
}

void Renderer::PresentScreen() {
	FrameBuffer::Unbind();

	PTGN_ASSERT(
		std::invoke([]() {
			auto viewport_size{ GLRenderer::GetViewportSize() };
			if (viewport_size.IsZero()) {
				return false;
			}
			if (viewport_size.x == 1 && viewport_size.y == 1) {
				return false;
			}
			return true;
		}),
		"Attempting to render to 0 or 1 sized viewport"
	);

	PTGN_ASSERT(
		FrameBuffer::IsUnbound(),
		"Frame buffer must be unbound (id=0) before swapping SDL2 buffer to the screen"
	);

	game.window.SwapBuffers();

	// TODO: Fix.
	// Flush();
	// screen_target_.Get().DrawToScreen();

	// TODO: Fix.
	// Screen target is cleared after drawing it to the screen.
	// TODO: Replace these with the Clear() function once that is added.
	/*screen_target_.Get().Bind();
	screen_target_.Get().Clear();*/

	/*
	// TODO: Move this to happen only when setting resolution. This would allow for example only one
	// render target to be drawn as resolution.
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
			GLRenderer::SetViewport(letterbox_size, game.window.GetSize() - 2.0f * letterbox_size);
			break;
		}
		case ResolutionMode::Overscan:	   break;
		case ResolutionMode::IntegerScale: break;
		default:						   PTGN_ERROR("Unrecognized resolution mode");
	}
	*/
}

void Renderer::ClearScreen() const {
	FrameBuffer::Unbind();
	GLRenderer::SetClearColor(background_color_);
	GLRenderer::Clear();
}

} // namespace ptgn::impl