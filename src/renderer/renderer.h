#pragma once

#include <functional>

#include "renderer/buffer.h"
#include "renderer/frame_buffer.h"
#include "renderer/shader.h"
#include "renderer/color.h"

namespace ptgn {

namespace impl {

struct Batch;
class RenderData;

class Renderer {
public:
	Renderer()							 = default;
	~Renderer()							 = default;
	Renderer(const Renderer&)			 = delete;
	Renderer(Renderer&&)				 = default;
	Renderer& operator=(const Renderer&) = delete;
	Renderer& operator=(Renderer&&)		 = default;

	void Clear();

	// Flushes all render layers.
	void Present();

	// Flush all render layers.
	void Flush();

	// Flush only a specific render layer. If the specified render layer does not have a primary
	// camera, the model view projection matrix will be an identity matrix.
	void Flush(std::size_t render_layer);

	void SetBlendMode(BlendMode blend_mode);
	[[nodiscard]] BlendMode GetBlendMode() const;

	// Sets the clear color of the currently bound render target.
	// Hence, prefer to call this in the Init function of a scene rather than the constructor as
	// this guarantees that the scene's render target is bound.
	void SetClearColor(const Color& color);
	[[nodiscard]] Color GetClearColor() const;
private:
	// Sets bound_frame_buffer_
	friend class FrameBuffer;
	friend class RenderData;
	friend struct impl::Batch;

	void Init();
	void Shutdown();
	void Reset();

	// TODO: Move to private and make Batch<> class friend.
	IndexBuffer quad_ib_;
	IndexBuffer triangle_ib_;
	IndexBuffer line_ib_;
	IndexBuffer point_ib_;
	IndexBuffer shader_ib_; // One set of quad indices.

	Color clear_color_{ color::Transparent };
	BlendMode blend_mode_{ BlendMode::Blend };

	Shader quad_shader_;
	Shader circle_shader_;
	Shader color_shader_;

	// Fade used with circle shader.
	const float fade_{ 0.005f };

	// Maximum number of primitive types before a second batch is generated.
	// The higher the number, the less draw calls but more RAM is used.
	std::size_t batch_capacity_{ 0 };
	
	std::uint32_t max_texture_slots_{ 0 };
	Texture white_texture_;

	FrameBuffer bound_frame_buffer_;
};

} // namespace impl

} // namespace ptgn