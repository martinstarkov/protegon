#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string_view>

#include "core/event/dispatcher.h"
#include "core/event/event.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/render_state.h"
#include "renderer/primitives/render_target.h"
#include "renderer/primitives/scaling_mode.h"
#include "renderer/primitives/shader.h"
#include "renderer/primitives/texture.h"
#include "renderer/primitives/viewport.h"

namespace ptgn {

class Application;
class EventHandler;
class RenderContext;
class DrawContext;
class Window;
class Scene;
class AssetManager;
class RenderTarget;
class Renderer;

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

class GLRenderer;

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

	Viewport GetDisplayViewport() const;

	/// @return The amount by which game size is scaled to achieve the display size.
	[[nodiscard]] V2_float GetScale() const;

	/// @return The game size of the renderer. Returns window size if unset.
	[[nodiscard]] V2_int GetGameSize() const;

	/// @return The game size scaling mode.
	[[nodiscard]] ScalingMode GetScalingMode() const;

	void SetBackgroundColor(Color background_color);
	[[nodiscard]] Color GetBackgroundColor() const;

private:
	friend class Application;
	friend class AssetManager;
	friend class EventHandler;
	friend class Scene;
	friend class RenderTarget;
	friend class DrawContext;
	friend class RenderContext;

	void BindScreenTarget();

	void SetViewport(Viewport viewport);

	void SetViewProjection(const Matrix4& view_projection);

	void SetBlend(BlendMode mode, bool enabled);

	void SetDepth(const DepthState& depth);

	void SetStencil(const StencilState& stencil);

	void SetRaster(const RasterState& raster);

	void SetScissor(const ScissorState& scissor);

	void SetColorMask(const ColorMaskState& color_mask);

	impl::RenderPass BeginPass(const impl::RenderTargetData& scene_target);

	impl::TextureId GetWhiteTexture() const;

	impl::ShaderId GetShader(std::string_view name) const;

	void DrawLine(
		impl::ShaderId shader, const std::array<V2_float, 2>& positions, Color tint, float depth
	);

	void DrawTriangle(
		impl::ShaderId shader, const std::array<V2_float, 3>& positions, Color tint, float depth
	);

	void DrawQuad(
		impl::ShaderId shader, const std::array<V2_float, 4>& positions,
		const std::array<float, 4>& user_data, Color tint, float depth
	);

	void DrawTexture(
		impl::ShaderId shader, impl::TextureId texture, const std::array<V2_float, 4>& positions,
		Color tint, float depth, const std::array<V2_float, 4>& tex_coords
	);

	impl::RenderTargetObject CreateRenderTarget(V2_int size, TextureFormat format);

	void OnEvent(EventDispatcher d);

	void BeginFrame();
	void EndFrame();

	Window& window_;

	// TODO: Figure out a way to decouple event emission (specifically with the user accessible
	// SetGameSize function, which can trigger DisplayResize events) from the renderer.
	EventHandler& events_;

	std::shared_ptr<impl::gl::GLRenderer> gl_renderer_;

	// emit_events = false is used to prevent emitting events when initializing the window and
	// scene.
	void UpdateDisplayViewport(V2_int window_size, bool emit_events = true);

	std::optional<V2_int> game_size_;
	Viewport display_viewport_;
	ScalingMode scaling_mode_{ ScalingMode::Letterbox };
};

} // namespace ptgn