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

class GLRenderer;
class VertexArray;

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

class Game;
class SceneManager;
class SceneCamera;
struct Batch;
struct RenderTargetInstance;
struct TextureInstance;
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
	void SetRenderTarget(const RenderTarget& target = {});

	// @return The current render target.
	[[nodiscard]] RenderTarget GetRenderTarget() const;

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
	friend class FrameBuffer;
	friend class RenderTarget;
	friend class RenderData;
	friend struct RenderTargetInstance;
	friend struct TextureInstance;
	friend struct Batch;
	friend class GLRenderer;
	friend class Game;
	friend class Texture;
	friend class Shader;
	friend class VertexArray;

	// Present the screen target to the window.
	void PresentScreen();

	// Clears the window buffer.
	void ClearScreen() const;

	void Init(const Color& window_background_color);
	void Shutdown();
	void Reset();

	// TODO: Consider moving everything from here to render_data_ into the RenderData class.

	template <BatchType T>
	const VertexArray& GetVertexArray() const {
		if constexpr (T == BatchType::Quad) {
			return quad_vao_;
		} else if constexpr (T == BatchType::Triangle) {
			return triangle_vao_;
		} else if constexpr (T == BatchType::Line) {
			return line_vao_;
		} else if constexpr (T == BatchType::Circle) {
			return circle_vao_;
		} else if constexpr (T == BatchType::Point) {
			return point_vao_;
		}
	}

	VertexArray quad_vao_;
	VertexArray circle_vao_;
	VertexArray triangle_vao_;
	VertexArray line_vao_;
	VertexArray point_vao_;

	// TODO: Move to private and make Batch<> class friend.
	IndexBuffer quad_ib_;
	IndexBuffer triangle_ib_;
	IndexBuffer line_ib_;
	IndexBuffer point_ib_;
	IndexBuffer shader_ib_; // One set of quad indices.

	Shader quad_shader_;
	Shader circle_shader_;
	Shader color_shader_;

	// Maximum number of primitive types before a second batch is generated.
	// The higher the number, the less draw calls but more RAM is used.
	std::size_t batch_capacity_{ 0 };

	std::uint32_t max_texture_slots_{ 0 };
	Texture white_texture_;

	RenderData render_data_;

	// Renderer keeps track of what is bound.
	FrameBuffer bound_frame_buffer_;
	Shader bound_shader_;
	VertexArray bound_vertex_array_;
	BlendMode bound_blend_mode_{ BlendMode::None };
	V2_int bound_viewport_position_;
	V2_int bound_viewport_size_;

	// Default value results in fullscreen.
	V2_int resolution_;
	ResolutionMode scaling_mode_{ ResolutionMode::Disabled };

	RenderTarget active_target_;
	RenderTarget screen_target_;
};

} // namespace impl

} // namespace ptgn