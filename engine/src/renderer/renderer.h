#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_pass.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/buffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "renderer/resources/vertex_array.h"
#include "renderer/vertex/vertex.h"

namespace ptgn {

class Application;
class RenderContext;
class DrawContext;
class DebugContext;
class Window;
class Scene;
class AssetManager;
class RenderTarget;
class Window;

namespace impl {

class Renderer;
class ShaderObject;
class TextureObject;
template <ResourceType T>
class Resource;
template <typename State, typename F>
	requires std::same_as<std::invoke_result_t<F&>, void>
void UpdateStateIfChanged(Renderer&, const std::optional<State>&, const State&, F&&);

namespace gl {

class GLContext;

} // namespace gl

using Index = std::uint32_t;

inline constexpr std::size_t kBatchCapacity{ 10000 };
inline constexpr std::size_t kVertexCapacity{ kBatchCapacity * 4 };
inline constexpr std::size_t kIndexCapacity{ kBatchCapacity * 6 };

struct QuadInfo {
	std::array<V2_float, 4> positions;
	std::array<V2_float, 4> tex_coords;
	Color color{ color::White };
	float depth{ 0.0f };
};

struct QuadDesc {
	QuadInfo quad;
	std::array<float, 4> user_data{};
};

struct TriangleParams {
	std::array<V2_float, 3> positions;
	Color tint{ color::White };
	float depth{ 0.0f };
};

struct QuadParams {
	QuadInfo quad;
	std::optional<TextureId> texture;
};

struct PooledTarget {
	RenderTargetObject target;
	std::uint64_t last_used_tick{ 0 };
	bool in_use{ false };
};

class Renderer {
public:
	ShaderId GetShader(std::string_view name) const;

	RenderTargetObject CreateRenderTarget(V2_int size, TextureFormat format);

	/// @return The display size of the renderer.
	V2_int GetDisplaySize() const;

	/// @return The game size of the renderer. Returns window size if unset.
	V2_int GetGameSize() const;

private:
	friend class ptgn::Application;

	Renderer() = delete;
	explicit Renderer(Window& window);
	~Renderer() noexcept;
	Renderer(const Renderer&)				 = delete;
	Renderer(Renderer&&) noexcept			 = delete;
	Renderer& operator=(const Renderer&)	 = delete;
	Renderer& operator=(Renderer&&) noexcept = delete;

	void BeginFrame();
	void EndFrame();

	/// @param game_size Setting to {} will use dynamic window size.
	void SetGameSize(
		std::optional<V2_int> game_size = {}, ScalingMode scaling_mode = ScalingMode::Letterbox
	);

	void SetScalingMode(ScalingMode scaling_mode = ScalingMode::Letterbox);

	Viewport GetDisplayViewport() const;

	/// @return The amount by which game size is scaled to achieve the display size.
	V2_float GetScale() const;

	/// @return The game size scaling mode.
	ScalingMode GetScalingMode() const;

	void SetBackgroundColor(Color background_color);
	Color GetBackgroundColor() const;

	template <typename State, typename F>
		requires std::same_as<std::invoke_result_t<F&>, void>
	friend void UpdateStateIfChanged(Renderer&, const std::optional<State>&, const State&, F&&);

	ShaderObject CreateShader(
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
	);

	TextureObject CreateTexture(const std::uint8_t* pixel_data, V2_int size, TextureFormat format);

	void SetUniform(ShaderId id, const char* uniform_name, const Matrix4& v);
	void SetUniform(ShaderId id, const char* uniform_name, float v);
	void SetUniform(ShaderId id, const char* uniform_name, V2_float v);
	void SetUniform(ShaderId id, const char* uniform_name, V3_float v);
	void SetUniform(ShaderId id, const char* uniform_name, V4_float v);
	void SetUniform(ShaderId id, const char* uniform_name, const std::vector<float>& v);
	void SetUniform(ShaderId id, const char* uniform_name, int v);
	void SetUniform(ShaderId id, const char* uniform_name, V2_int v);
	void SetUniform(ShaderId id, const char* uniform_name, V3_int v);
	void SetUniform(ShaderId id, const char* uniform_name, V4_int v);
	void SetUniform(ShaderId id, const char* uniform_name, const std::vector<int>& v);
	void SetUniform(ShaderId id, const char* uniform_name, bool v);

	/// @return The texture slot the given texture is bound to, and whether it should be pushed to
	/// batch_textures.
	std::pair<std::uint32_t, bool> GetTextureSlot(TextureId tex);

	V2_int GetTextureSize(TextureId id) const;
	TextureFormat GetTextureFormat(TextureId id) const;

	void Destroy(VertexBufferId id);
	void Destroy(ElementBufferId id);
	void Destroy(UniformBufferId id);
	void Destroy(ShaderId id);
	void Destroy(TextureId id);
	void Destroy(RenderbufferId id);
	void Destroy(FramebufferId id);
	void Destroy(VertexArrayId id);
	void Destroy(RenderTargetId id);

	TextureId GetRenderTargetTexture(RenderTargetId render_target) const;
	V2_int GetRenderTargetSize(RenderTargetId render_target) const;
	TextureFormat GetRenderTargetTextureFormat(RenderTargetId render_target) const;
	void ResizeRenderTarget(RenderTargetId render_target, V2_int new_size);
	void ClearRenderTarget(RenderTargetId render_target, Color color, bool set_viewport) const;
	void BindRenderTarget(RenderTargetId render_target);
	void BindRenderPass(RenderPass& render_pass);

	/// @brief Flushes the batch if adding the given number of vertices and indices would exceed
	/// batch.
	void FlushIfExceedsCapacity(std::size_t vertices, std::size_t indices);
	void FlushBatch();

	void SetViewport(Viewport viewport);
	void SetShader(ShaderId shader);
	void SetViewProjection(const Matrix4& view_projection);
	void SetFramebuffer(FramebufferId framebuffer);
	void SetBlend(bool enabled);
	void SetBlendMode(BlendMode mode);
	void SetDepthTesting(bool enabled);
	void SetDepthMask(const DepthMaskState& mask);
	void SetStencil(const StencilState& stencil);
	void SetRaster(const RasterState& raster);
	void SetScissor(const ScissorState& scissor);
	void SetColorMask(const ColorMaskState& color_mask);

	TextureId GetWhiteTexture() const;

	void DrawTriangle(
		ShaderId shader, const std::array<V2_float, 3>& positions, Color tint, float depth
	);

	void DrawQuad(
		ShaderId shader, const std::array<V2_float, 4>& positions,
		const std::array<float, 4>& user_data, Color tint, float depth,
		const std::function<void()>& shader_setup
	);

	void DrawTexture(
		ShaderId shader, TextureId texture, const std::array<V2_float, 4>& positions, Color tint,
		float depth, const std::array<V2_float, 4>& tex_coords,
		const std::function<void()>& shader_setup
	);

	void DrawTexture(
		ShaderId shader, RenderPass& pass, RenderTargetId scene_render_target,
		const std::function<void()>& shader_setup
	);

	/// @param setup Returns true if the renderer should flush the batch after adding the quad
	/// params. This allows shader uniforms to be applied to each unique quad in the batch.
	void DrawQuad(
		ShaderId shader, const QuadParams& p, const std::function<bool(ShaderId, QuadDesc&)>& setup
	);

	/// @return True if the given texture is currently attached to the framebuffer that is currently
	/// bound.
	bool IsTextureAttachedToCurrentFramebuffer(TextureId texture) const;

	void BindScreenTarget();
	void ResizeScreenTarget(V2_int size);

	void OnWindowResize(V2_int size);

	RenderPass BeginPass(RenderTargetId scene_render_target);

	RenderTargetId AcquirePooledTargetCopy(RenderTargetId render_target);
	RenderTargetId AcquirePooledTarget(V2_int size, TextureFormat format);

	void ReleasePooledTarget(RenderTargetId render_target);

	void InvalidateState();

	Window& window_;

	std::function<void(V2_int, ResizeType)> event_sink_;

	std::unique_ptr<gl::GLContext> gl_;

	// emit_events = false is used to prevent emitting events when initializing the window and
	// scene.
	void UpdateDisplayViewport(V2_int window_size, bool emit_events = true);

	VertexBufferObject vbo_;
	ElementBufferObject ebo_;
	VertexArrayObject vao_;
	TextureObject white_texture_;

	std::vector<Vertex> batch_vertices_;
	std::vector<Index> batch_indices_;
	std::vector<TextureId> batch_textures_;

	Matrix4 view_projection_;

	Color background_color_;
	RenderTargetObject screen_target_;

	std::optional<V2_int> game_size_;
	Viewport display_viewport_;
	ScalingMode scaling_mode_{ ScalingMode::Letterbox };

	std::vector<PooledTarget> rt_pool_;
	std::uint64_t pool_tick_{ 0 };
	std::size_t max_pool_size_{ 16 };
};

} // namespace impl

} // namespace ptgn