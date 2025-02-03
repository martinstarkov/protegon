#pragma once

#include <cstdint>
#include <functional>

#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/batch.h"
#include "renderer/buffer.h"
#include "renderer/color.h"
#include "renderer/render_data.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"

namespace ptgn {

class Shader;
class FrameBuffer;

// How the renderer resolution is scaled to the window size.
enum class ResolutionMode {
	Disabled,  /**< There is no scaling in effect */
	Stretch,   /**< The rendered content is stretched to the output resolution */
	Letterbox, /**< The rendered content is fit to the largest dimension and the other dimension is
				  letterboxed with black bars */
	Overscan,  /**< The rendered content is fit to the smallest dimension and the other dimension
				  extends beyond the output bounds */
	IntegerScale, /**< The rendered content is scaled up by integer multiples to fit the output
					 resolution */
};

namespace impl {

class VertexArray;
class Game;
class SceneManager;
class SceneCamera;
struct Batch;
class GLRenderer;
struct RenderTargetInstance;
struct TextureInstance;
struct ShaderInstance;
class RenderData;
class InputHandler;

class Renderer {
public:
	Renderer()							 = default;
	~Renderer()							 = default;
	Renderer(const Renderer&)			 = delete;
	Renderer(Renderer&&)				 = default;
	Renderer& operator=(const Renderer&) = delete;
	Renderer& operator=(Renderer&&)		 = default;

	// Sets the current render target. Every subsequent object draw call will be drawn to this
	// render target. In order to see the render target on the screen, first call
	// SetRenderTarget({}) to set the screen target followed by target.Draw() to draw the target to
	// the screen.
	// @param target The desired render target to be set. If {}, the screen target will be set.
	// Note if provided target is not the currently set render target, this function will flush the
	// renderer.
	// TODO: Fix.
	// void SetRenderTarget(const RenderTarget& target = {});

	// @return The current render target.
	//[[nodiscard]] RenderTarget GetRenderTarget() const;

	// Clear the current render target.
	void Clear() const;

	// Flush the render queue onto the current render target.
	void Flush();

	// @param The blend mode to set for the current render target.
	// Note: If this blend mode is different from the blend mode of the currently set render target,
	// this function will flush the renderer.
	void SetBlendMode(BlendMode blend_mode);

	// @return The blend mode of the current render target.
	[[nodiscard]] BlendMode GetBlendMode() const;

	// Sets the clear color of the current render target.
	// Note: The newly set clear color will only be applied upon clearing the render target. This
	// happens after calling Draw() on the render target, or for the screen target at the end of the
	// current frame.
	void SetClearColor(const Color& clear_color);

	// @return The clear color of the current render target.
	[[nodiscard]] Color GetClearColor() const;

	// Sets the viewport for the current render target.
	// @param viewport Where to draw the current render target.
	void SetViewport(const Rect& viewport);

	// @return Viewport of the current render target.
	[[nodiscard]] Rect GetViewport() const;

	// @param resolution The resolution size to which the renderer will be displayed.
	// Note: If no resolution mode is set, setting the resolution will default it to
	// ResolutionMode::Stretch.
	// Note: Setting this will override a set viewport.
	void SetResolution(const V2_int& resolution);

	// @param mode The mode in which to fit the resolution to the window. If
	// ResolutionMode::Disabled, the resolution is ignored.
	// Note: Setting this will override a set viewport.
	void SetResolutionMode(ResolutionMode mode);

	// @return The resolution size of the renderer. If resolution has not been set, returns window
	// size.
	[[nodiscard]] V2_int GetResolution() const;

	// @return The resolution scaling mode.
	[[nodiscard]] ResolutionMode GetResolutionMode() const;

	// @return The render data associated with the current render queue.
	[[nodiscard]] impl::RenderData& GetRenderData();

private:
	friend class ptgn::Shader;
	friend class VertexArray;
	friend class ptgn::FrameBuffer;

	friend class ptgn::RenderTarget;
	friend class GLRenderer;
	friend class Game;
	friend class RenderData;
	friend struct RenderTargetInstance;
	friend struct ShaderInstance;
	friend struct TextureInstance;
	friend struct Batch;

	// Present the screen target to the window.
	void PresentScreen();

	// Clears the window buffer.
	void ClearScreen() const;

	void Init(const Color& window_background_color);
	void Shutdown();
	void Reset();

	RenderData render_data_;

	// Renderer keeps track of what is bound.
	std::uint32_t bound_frame_buffer_id_{ 0 };
	std::uint32_t bound_shader_id_{ 0 };
	std::uint32_t bound_vertex_array_id_{ 0 };
	BlendMode bound_blend_mode_{ BlendMode::None };
	V2_int bound_viewport_position_;
	V2_int bound_viewport_size_;

	// Default value results in fullscreen.
	V2_int resolution_;
	ResolutionMode scaling_mode_{ ResolutionMode::Disabled };

	// TODO: Fix.
	/*RenderTarget current_target_;
	RenderTarget screen_target_;*/
};

} // namespace impl

} // namespace ptgn