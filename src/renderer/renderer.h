#pragma once

#include <functional>

#include "renderer/buffer.h"
#include "renderer/color.h"
#include "renderer/render_target.h"

namespace ptgn {

class FrameBuffer;

class Scene;
class Texture;
class Text;
class VertexArray;
class Shader;
struct Line;
struct Capsule;
struct Ellipse;
struct Rect;
struct Polygon;
struct Arc;
struct RoundedRect;
struct Axis;
struct Circle;
struct Triangle;

namespace impl {

class Batch;

class CameraManager;
class Game;
struct RenderLayer;
class TextureBatchData;
struct RenderData;
struct Point;

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

	// TODO: Move to private and make Batch<> class friend.
	[[nodiscard]] std::size_t GetBatchCapacity() const;

	// Sets render target back to the screen.
	void ResetRenderTarget();

	// TODO: Move to private and make Batch<> class friend.
	IndexBuffer quad_ib_;
	IndexBuffer triangle_ib_;
	IndexBuffer line_ib_;
	IndexBuffer point_ib_;
	IndexBuffer shader_ib_; // One set of quad indices.
private:
	// Sets bound_frame_buffer_
	friend class FrameBuffer;
	friend class impl::Batch;

	friend class CameraManager;
	friend class Game;
	friend class impl::TextureBatchData;
	friend struct impl::RenderData;
	friend class Scene;
	friend class Shader;
	// TODO: Think of a better way to do this.
	friend class Text;
	friend class Texture;
	friend struct Line;
	friend struct Capsule;
	friend struct Triangle;
	friend struct Ellipse;
	friend struct Rect;
	friend struct Polygon;
	friend struct Arc;
	friend struct RoundedRect;
	friend struct Axis;
	friend struct Circle;
	friend struct impl::Point;

	void Init();
	void Shutdown();
	void Reset();

	Color clear_color_{ color::Transparent };
	BlendMode blend_mode_{ BlendMode::Blend };

	ptgn::Shader quad_shader_;
	ptgn::Shader circle_shader_;
	ptgn::Shader color_shader_;

	// Fade used with circle shader.
	const float fade_{ 0.005f };
	std::size_t batch_capacity_{ 0 };
	std::uint32_t max_texture_slots_{ 0 };
	Texture white_texture_;

	RenderData data_;

	RenderTarget screen_target_;

	FrameBuffer bound_frame_buffer_;
};

} // namespace impl

} // namespace ptgn