#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "renderer/backend/gl/gl_buffer.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/camera/viewport.h"
#include "renderer/resources/render_state.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/vertex.h"
#include "renderer/targets/render_target.h"

namespace ptgn {

class Renderer;
class Window;

namespace impl::gl {

class Renderer;
class GLContext;

template <class State, class Func>
void UpdateStateIfChanged(Renderer&, const State&, const State&, Func&&);

using Index = std::uint32_t;

constexpr std::size_t batch_capacity{ 10000 };
constexpr std::size_t vertex_capacity{ batch_capacity * 4 };
constexpr std::size_t index_capacity{ batch_capacity * 6 };

struct QuadDesc {
	std::array<V2_float, 4> positions;
	std::array<V2_float, 4> tex_coords;
	Color color{ color::White };
	float rotation{ 0.0f };
	std::array<float, 4> user_data{};
};

struct PooledTarget {
	RenderTarget target;
	std::uint64_t last_used_tick{ 0 };
	bool in_use{ false };
};

struct QuadParams {
	V2_float center{};
	V2_float size{ 0.0f, 0.0f };
	float rotation{ 0.0f };

	bool flip_y{ false };

	Color tint{ color::White };

	std::optional<Texture> texture;

	std::optional<std::array<V2_float, 4>> tex_coords;
};

class Renderer {
public:
	Renderer() = delete;
	explicit Renderer(Window& window);
	~Renderer() noexcept;
	Renderer(const Renderer&)				 = delete;
	Renderer(Renderer&&) noexcept			 = delete;
	Renderer& operator=(const Renderer&)	 = delete;
	Renderer& operator=(Renderer&&) noexcept = delete;

	RenderTarget CreateRenderTarget(V2_int size, TextureFormat format) const;
	void ResizeRenderTarget(RenderTarget& rt, V2_int new_size) const;
	void BindRenderTarget(const RenderTarget& rt);
	void BindRenderTarget(RenderPass& pass);
	void ClearRenderTarget(const RenderTarget& rt, Color color);

	void DrawTexture(
		Shader shader, Texture texture, V2_float center, V2_float size, Color tint = color::White,
		bool flip_y = false
	);
	void DrawTexture(
		const RenderTarget& rt, V2_float center, V2_float size, Color tint = color::White,
		bool flip_y = false
	);
	void DrawTexture(
		Texture texture, V2_float center, V2_float size, Color tint = color::White,
		bool flip_y = false
	);
	void DrawTexture(Shader shader, RenderPass& pass, const RenderTarget& scene_target);

	void SetViewProjection(const Matrix4& view_projection);
	void SetShader(Shader shader);
	void SetBlend(BlendMode mode, bool enabled = true);
	void SetFramebuffer(Framebuffer framebuffer, const Viewport& viewport);
	void SetDepth(const DepthState& depth);
	void SetStencil(const StencilState& stencil);
	void SetRaster(const RasterState& raster);
	void SetColorMask(const ColorMaskState& color_mask);

	RenderPass BeginPass(const RenderTarget& scene_target);

	V2_int GetTextureSize(Texture texture) const;

	// TODO: Move to private.
	std::unique_ptr<GLContext> gl_;
	// TODO: Move to private.
	RenderTarget screen_target_;

private:
	friend class Application;
	friend class RenderPass;
	friend class ptgn::Renderer;
	template <class State, class Func>
	friend void UpdateStateIfChanged(Renderer&, const State&, const State&, Func&&);

	using QuadSetup = std::function<void(Shader, QuadDesc&)>;

	void DrawQuad(Shader shader, const QuadParams& p, const QuadSetup& q);

	void BeginFrame();
	void EndFrame(const Viewport& viewport);

	void FlushBatch();

	std::uint32_t GetTextureSlot(Texture tex);

	RenderTarget AcquirePooledTarget(V2_int size, TextureFormat format);
	void ReleasePooledTarget(const RenderTarget& target);

	std::vector<Vertex> batch_vertices_;
	std::vector<Index> batch_indices_;
	std::vector<Texture> batch_textures_;

	Matrix4 view_projection_;

	VertexBuffer vbo_;
	ElementBuffer ebo_;
	VertexArray vao_;
	Texture white_texture_;

	std::vector<PooledTarget> rt_pool_;
	std::uint64_t pool_tick_{ 0 };
	std::size_t max_pool_size_{ 16 };
};

} // namespace impl::gl

} // namespace ptgn