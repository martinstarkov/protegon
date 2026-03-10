#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <vector>

#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/buffer.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/framebuffer.h"
#include "renderer/primitives/render_state.h"
#include "renderer/primitives/render_target.h"
#include "renderer/primitives/shader.h"
#include "renderer/primitives/texture.h"
#include "renderer/primitives/vertex.h"
#include "renderer/primitives/vertex_array.h"
#include "renderer/primitives/viewport.h"

namespace ptgn {

class Window;

namespace impl {

class RenderPass;

} // namespace impl

namespace impl::gl {

class GLContext;

template <typename State, typename F>
	requires std::same_as<std::invoke_result_t<F&>, void>
bool UpdateStateIfChanged(GLRenderer&, const State&, const State&, F&&);

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
	RenderTargetObject target;
	std::uint64_t last_used_tick{ 0 };
	bool in_use{ false };
};

struct TriangleParams {
	std::array<V2_float, 3> positions;
	Color tint{ color::White };
	float depth{ 0.0f };
};

struct QuadParams {
	std::array<V2_float, 4> positions;
	float depth{ 0.0f };
	Color tint{ color::White };
	std::optional<TextureId> texture;
	std::array<V2_float, 4> tex_coords;
};

class GLRenderer {
public:
	GLRenderer() = delete;
	explicit GLRenderer(Window& window);
	~GLRenderer() noexcept;
	GLRenderer(const GLRenderer&)				 = delete;
	GLRenderer(GLRenderer&&) noexcept			 = delete;
	GLRenderer& operator=(const GLRenderer&)	 = delete;
	GLRenderer& operator=(GLRenderer&&) noexcept = delete;

	RenderTargetObject CreateRenderTarget(V2_int size, TextureFormat format);

	void DrawLine(
		ShaderId shader, const std::array<V2_float, 2>& positions, Color tint, float depth
	);
	void DrawTriangle(
		ShaderId shader, const std::array<V2_float, 3>& positions, Color tint, float depth
	);
	void DrawQuad(
		ShaderId shader, const std::array<V2_float, 4>& positions,
		const std::array<float, 4>& user_data, Color tint, float depth
	);
	void DrawTexture(
		ShaderId shader, TextureId texture, const std::array<V2_float, 4>& positions, Color tint,
		float depth, const std::array<V2_float, 4>& tex_coords
	);
	void DrawTexture(ShaderId shader, RenderPass& pass, const RenderTargetData& scene_target);

	bool SetViewport(Viewport viewport);
	bool SetViewProjection(const Matrix4& view_projection);
	bool SetShader(ShaderId shader);
	bool SetBlend(BlendMode mode, bool enabled = true);
	bool SetFramebuffer(FramebufferId framebuffer);
	bool SetDepth(const DepthState& depth);
	bool SetStencil(const StencilState& stencil);
	bool SetRaster(const RasterState& raster);
	bool SetScissor(const ScissorState& scissor);
	bool SetColorMask(const ColorMaskState& color_mask);

	ShaderId GetShader(std::string_view name) const;

	RenderPass BeginPass(const RenderTargetData& scene_target);

	V2_int GetTextureSize(TextureId texture) const;

	void BeginFrame(V2_int window_size, Color window_background_color);
	void EndFrame(Viewport display_viewport);

	TextureId GetWhiteTexture() const;

	void SetBackgroundColor(Color background_color);
	[[nodiscard]] Color GetBackgroundColor() const;

	void ResizeScreenTarget(V2_int size);
	void BindScreenTarget();

	std::unique_ptr<GLContext> gl;

private:
	friend class ptgn::impl::RenderPass;
	template <typename State, typename F>
		requires std::same_as<std::invoke_result_t<F&>, void>
	friend bool UpdateStateIfChanged(GLRenderer&, const State&, const State&, F&&);

	using QuadSetup = std::function<void(ShaderId, QuadDesc&)>;

	/// @return True if the given texture is currently attached to the framebuffer that is currently
	/// bound.
	bool IsTextureAttachedToCurrentFramebuffer(TextureId texture) const;

	void DrawQuad(ShaderId shader, const QuadParams& p, const QuadSetup& q);

	void FlushBatch();

	std::uint32_t GetTextureSlot(TextureId tex);

	RenderTargetData AcquirePooledTarget(V2_int size, TextureFormat format);
	void ReleasePooledTarget(RenderTargetData& target);

	ClearColor background_color_;
	RenderTargetObject screen_target_;
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