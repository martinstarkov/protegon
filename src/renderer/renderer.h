#pragma once

#include <cstdint>

#include "render_data.h"
#include "renderer/batch.h"
#include "renderer/buffer.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"

namespace ptgn {

class GLRenderer;
struct LayerInfo;

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

	void Clear() const;

	void Present();

	void Flush();

	void SetBlendMode(BlendMode blend_mode);
	[[nodiscard]] BlendMode GetBlendMode() const;

	// Sets the clear color of the currently bound render target.
	// Hence, prefer to call this in the Init function of a scene rather than the constructor as
	// this guarantees that the scene's render target is bound.
	void SetClearColor(const Color& clear_color);
	[[nodiscard]] Color GetClearColor() const;

	// @return Render target for the the scene which is currently in the process of being updated,
	// initialized, or shutdown (i.e. inside of the scene's Init(), Update(), Shutdown() functions).
	// If outside of those functions, returns the screen target.
	[[nodiscard]] RenderTarget GetCurrentRenderTarget() const;

private:
	// Sets bound_frame_buffer_
	friend class ptgn::FrameBuffer;
	friend class RenderData;
	friend struct Batch;
	friend class Game;
	friend class SceneManager;
	friend struct LayerInfo;
	friend class GLRenderer;
	friend class ptgn::RenderTarget;
	friend class SceneCamera;
	friend class Shader;
	friend class ptgn::GLRenderer;
	friend class impl::InputHandler;

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

	void ClearScreen() const;
	void Init(const Color& background_color);
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

	std::uint32_t max_texture_slots_{ 0 };
	Texture white_texture_;

	// Renderer keeps track of what is bound.
	FrameBuffer bound_frame_buffer_;
	Shader bound_shader_;

	RenderTarget screen_target_;
};

} // namespace impl

} // namespace ptgn