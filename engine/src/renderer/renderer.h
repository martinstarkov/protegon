#pragma once

#include <memory>
#include <optional>

#include "core/event/dispatcher.h"
#include "core/event/event.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "renderer/camera/scaling_mode.h"
#include "renderer/camera/viewport.h"
#include "renderer/resources/render_state.h"
#include "renderer/resources/texture.h"
#include "renderer/targets/render_target.h"

namespace ptgn {

class Application;
class EventHandler;
class Window;

struct GameResized : public Event<GameResized> {
	V2_int size;
};

namespace impl {

struct DisplayResized : public Event<DisplayResized> {
	V2_int size;
};

struct DisplayViewportChanged : public Event<DisplayViewportChanged> {
	Viewport viewport;
};

namespace gl {

class Renderer;

} // namespace gl

} // namespace impl

class Renderer {
public:
	Renderer() = delete;
	explicit Renderer(Window& window, EventHandler& events);
	~Renderer() noexcept;
	Renderer(const Renderer&)				 = delete;
	Renderer(Renderer&&) noexcept			 = delete;
	Renderer& operator=(const Renderer&)	 = delete;
	Renderer& operator=(Renderer&&) noexcept = delete;

	/// @param game_size Setting to {} will use dynamic window size.
	void SetGameSize(
		std::optional<V2_int> game_size = {}, ScalingMode scaling_mode = ScalingMode::Letterbox
	);

	void SetScalingMode(ScalingMode scaling_mode = ScalingMode::Letterbox);

	/// @return The display size of the renderer.
	[[nodiscard]] V2_int GetDisplaySize() const;

	/// @return The amount by which game size is scaled to achieve the display size.
	[[nodiscard]] V2_float GetScale() const;

	/// @return The game size of the renderer. Returns window size if unset.
	[[nodiscard]] V2_int GetGameSize() const;

	/// @return The game size scaling mode.
	[[nodiscard]] ScalingMode GetScalingMode() const;

	void DrawTexture(
		const RenderTarget& rt, V2_float center, V2_float size, Color tint = color::White,
		bool flip_y = true
	);
	void DrawTexture(Texture texture, V2_float center, V2_float size, Color tint = color::White);
	void DrawRect(V2_float center, V2_float size, Color color);

	RenderTarget GetScreenTarget() const;
	RenderTarget CreateRenderTarget(V2_int size, TextureFormat format) const;
	void ResizeRenderTarget(RenderTarget& rt, V2_int new_size) const;
	void BindRenderTarget(const RenderTarget& rt);
	void BindRenderTarget(RenderPass& pass);
	void ClearRenderTarget(const RenderTarget& rt, Color color = color::Transparent);

	void SetViewProjection(const Matrix4& view_projection);
	void SetBlend(BlendMode mode, bool enabled = true);
	void SetDepth(const DepthState& depth);
	void SetStencil(const StencilState& stencil);
	void SetRaster(const RasterState& raster);
	void SetColorMask(const ColorMaskState& color_mask);

	RenderPass BeginPass(const RenderTarget& scene_target);

	V2_int GetTextureSize(Texture texture) const;

private:
	friend class Application;
	friend class EventHandler;

	void OnEvent(EventDispatcher d);

	void BeginFrame();
	void EndFrame();

	Window& window_;

	// TODO: Figure out a way to decouple event emission (specifically with the user accessible
	// SetGameSize function, which can trigger DisplayResize events) from the renderer.
	EventHandler& events_;

	std::shared_ptr<impl::gl::Renderer> gl_renderer_;

	// emit_events = false is used to prevent emitting events when initializing the window and
	// scene.
	void UpdateDisplayViewport(V2_int window_size, bool emit_events = true);

	std::optional<V2_int> game_size_;
	Viewport display_viewport_;
	ScalingMode scaling_mode_{ ScalingMode::Letterbox };
};

} // namespace ptgn