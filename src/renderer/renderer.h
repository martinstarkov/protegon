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

	// Temporarily changes the current render target to the one specified until the callback
	// finishes executing, then sets the current render target and blend mode back to what they were
	// before this function was called.
	void SetTemporaryRenderTarget(
		const RenderTarget& render_target, const std::function<void()>& callback
	);

	// Temporarily changes the blend mode to the specified until the callback finishes
	// executing, then sets the blend mode back to what they were before
	// this function was called.
	void SetTemporaryBlendMode(BlendMode blend_mode, const std::function<void()>& callback);

	// Clear the currently set render target.
	void Clear() const;

	// Flush the render queue onto the currently set render target.
	void Flush();

	// @param viewport Where to draw the current render target. Default value of {} results in
	// fullscreen viewport.
	void SetViewport(const Rect& viewport = {});

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

	// @param The blend mode to set for the current render queue.
	void SetBlendMode(BlendMode blend_mode);

	// @return The blend mode which is currently set.
	[[nodiscard]] BlendMode GetBlendMode() const;

	// Sets the clear color of the currently set render target.
	void SetClearColor(const Color& clear_color);

	// @return The clear color of the currently set render target.
	[[nodiscard]] Color GetClearColor() const;

	// Sets the renderer render target.
	// @param target The desired render target to be set. If {}, renderer will draw directly to the
	// screen target.
	void SetRenderTarget(const RenderTarget& target = {});

	// @return The currently set render target. Will return {} if the screen target is currently
	// set.
	[[nodiscard]] RenderTarget GetRenderTarget() const;

	// @return The render data associated with the current renderer queue.
	[[nodiscard]] impl::RenderData& GetRenderData();

private:
	// Sets bound_frame_buffer_
	friend class FrameBuffer;
	friend class RenderTarget;
	friend class RenderData;
	friend struct Batch;
	friend class Game;
	friend class Texture;
	friend class Shader;

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

	// Present the screen target to the window.
	void Present();

	// Clear the screen target.
	void ClearScreen() const;

	void Init(const Color& window_background_color);
	void Shutdown();
	void Reset();

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

	RenderData render_data_;

	std::uint32_t max_texture_slots_{ 0 };
	Texture white_texture_;

	// Renderer keeps track of what is bound.
	FrameBuffer bound_frame_buffer_;
	Shader bound_shader_;

	// Currently set blend mode.
	BlendMode blend_mode_{ BlendMode::BlendPremultiplied };

	// Default value results in fullscreen.
	V2_int resolution_;
	ResolutionMode scaling_mode_{ ResolutionMode::Disabled };

	RenderTarget active_target_;
	RenderTarget screen_target_;
};

} // namespace impl

} // namespace ptgn