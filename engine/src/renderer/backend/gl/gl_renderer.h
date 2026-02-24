#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "renderer/camera/viewport.h"
#include "renderer/resources/buffer.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/render_state.h"
#include "renderer/resources/render_target.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/vertex.h"
#include "renderer/resources/vertex_array.h"

namespace ptgn {

class Window;

namespace impl {

class RenderPass;

} // namespace impl

namespace impl::gl {

class GLContext;

template <class State, class Func>
void UpdateStateIfChanged(Renderer&, const State&, const State&, Func&&);

using Index = std::uint32_t;

inline constexpr std::size_t kBatchCapacity{ 10000 };
inline constexpr std::size_t kVertexCapacity{ kBatchCapacity * 4 };
inline constexpr std::size_t kIndexCapacity{ kBatchCapacity * 6 };

struct QuadDesc {
	std::array<V2_float, 4> positions;
	std::array<V2_float, 4> tex_coords;
	Color color{ color::White };
	float depth{ 0.0f };
	std::array<float, 4> user_data{};
};

struct PooledTarget {
	RenderTarget target;
	std::uint64_t last_used_tick{ 0 };
	bool in_use{ false };
};

struct QuadParams {
	std::array<V2_float, 4> positions;

	float depth{ 0.0f };

	bool flip_y{ false };

	Color tint{ color::White };

	std::optional<TextureId> texture;

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

	RenderTarget CreateRenderTarget(V2_int size, TextureFormat format);

	void DrawTexture(
		ShaderId shader, TextureId texture, const std::array<V2_float, 4>& positions,
		Color tint = color::White, float depth = 0.0f, bool flip_y = false,
		const std::optional<std::array<V2_float, 4>>& tex_coords = {}
	);
	void DrawTexture(ShaderId shader, RenderPass& pass, const RenderTargetData& scene_target);

	void SetViewport(Viewport viewport);
	void SetViewProjection(const Matrix4& view_projection);
	void SetShader(ShaderId shader);
	void SetBlend(BlendMode mode, bool enabled = true);
	void SetFramebuffer(FramebufferId framebuffer);
	void SetDepth(const DepthState& depth);
	void SetStencil(const StencilState& stencil);
	void SetRaster(const RasterState& raster);
	void SetColorMask(const ColorMaskState& color_mask);

	ShaderId GetShader(std::string_view name) const;

	RenderPass BeginPass(const RenderTargetData& scene_target);

	V2_int GetTextureSize(TextureId texture) const;

	const RenderTargetData& GetScreenTarget() const;
	RenderTargetData& GetScreenTarget();

	void BeginFrame(V2_int window_size);
	void EndFrame(Viewport display_viewport);

	TextureId GetWhiteTexture() const;

	std::unique_ptr<GLContext> gl;

private:
	friend class Application;
	friend class ptgn::impl::RenderPass;
	template <class State, class Func>
	friend void UpdateStateIfChanged(Renderer&, const State&, const State&, Func&&);

	using QuadSetup = std::function<void(ShaderId, QuadDesc&)>;

	void DrawQuad(ShaderId shader, const QuadParams& p, const QuadSetup& q);

	void FlushBatch();

	std::uint32_t GetTextureSlot(TextureId tex);

	RenderTargetData AcquirePooledTarget(V2_int size, TextureFormat format);
	void ReleasePooledTarget(RenderTargetData& target);

	RenderTarget screen_target_;
	VertexBufferObject vbo_;
	ElementBufferObject ebo_;
	VertexArrayObject vao_;
	TextureObject white_texture_;

	std::vector<Vertex> batch_vertices_;
	std::vector<Index> batch_indices_;
	std::vector<TextureId> batch_textures_;

	Matrix4 view_projection_;

	std::vector<PooledTarget> rt_pool_;
	std::uint64_t pool_tick_{ 0 };
	std::size_t max_pool_size_{ 16 };
};

} // namespace impl::gl

} // namespace ptgn