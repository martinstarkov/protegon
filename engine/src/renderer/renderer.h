#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "core/event/dispatcher.h"
#include "core/event/event.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/buffer.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/render_state.h"
#include "renderer/primitives/render_target.h"
#include "renderer/primitives/resource.h"
#include "renderer/primitives/scaling_mode.h"
#include "renderer/primitives/shader.h"
#include "renderer/primitives/texture.h"
#include "renderer/primitives/texture_format.h"
#include "renderer/primitives/vertex.h"
#include "renderer/primitives/vertex_array.h"
#include "renderer/primitives/viewport.h"

namespace ptgn {

class Application;
class EventHandler;
class RenderContext;
class DrawContext;
class DebugContext;
class Window;
class Scene;
class AssetManager;
class RenderTarget;
class Renderer;

struct GameResized : public Event<GameResized> {
	GameResized() = default;

	explicit GameResized(V2_int game_size) : size{ game_size } {}

	V2_int size;
};

namespace impl {

class ShaderObject;
class SDLInstance;
class RenderTargetObject;
class TextureObject;

template <ResourceType T>
class Resource;

template <typename State, typename F>
	requires std::same_as<std::invoke_result_t<F&>, void>
void UpdateStateIfChanged(Renderer&, const State&, const State&, F&&);

struct InternalGameResized : public Event<InternalGameResized> {
	InternalGameResized() = default;

	explicit InternalGameResized(V2_int game_size) : size{ game_size } {}

	V2_int size;
};

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

struct InternalDisplayResized : public Event<InternalDisplayResized> {
	V2_int size;
};

struct InternalDisplayViewportChanged : public Event<InternalDisplayViewportChanged> {
	Viewport viewport;
};

struct PooledTarget {
	RenderTargetObject target;
	std::uint64_t last_used_tick{ 0 };
	bool in_use{ false };
};

namespace gl {

class GLContext;

} // namespace gl

using Index = std::uint32_t;

inline constexpr std::size_t kBatchCapacity{ 10000 };
inline constexpr std::size_t kVertexCapacity{ kBatchCapacity * 4 };
inline constexpr std::size_t kIndexCapacity{ kBatchCapacity * 6 };

} // namespace impl

class Renderer {
private:
	friend class Application;
	friend class AssetManager;
	friend class EventHandler;
	friend class Scene;
	friend class RenderTarget;
	friend class DrawContext;
	friend class RenderContext;
	friend class DebugContext;
	friend class impl::ShaderObject;
	friend class impl::RenderTargetObject;
	friend class impl::TextureObject;
	friend class impl::SDLInstance;
	template <impl::ResourceType T>
	friend class impl::Resource;

	Renderer() = delete;
	explicit Renderer(Window& window, EventHandler& events);
	~Renderer() noexcept;
	Renderer(const Renderer&)				 = delete;
	Renderer(Renderer&&) noexcept			 = delete;
	Renderer& operator=(const Renderer&)	 = delete;
	Renderer& operator=(Renderer&&) noexcept = delete;

	/// @param game_size Setting to {} will use dynamic window size.
	void SetGameSize(
		std::optional<V2_int> game_size = {}, ScalingMode scaling_mode = ScalingMode::Letterbox
	);

	void SetScalingMode(ScalingMode scaling_mode = ScalingMode::Letterbox);

	/// @return The display size of the renderer.
	V2_int GetDisplaySize() const;

	Viewport GetDisplayViewport() const;

	/// @return The amount by which game size is scaled to achieve the display size.
	V2_float GetScale() const;

	/// @return The game size of the renderer. Returns window size if unset.
	V2_int GetGameSize() const;

	/// @return The game size scaling mode.
	ScalingMode GetScalingMode() const;

	void SetBackgroundColor(Color background_color);
	Color GetBackgroundColor() const;

	template <typename State, typename F>
		requires std::same_as<std::invoke_result_t<F&>, void>
	friend void impl::UpdateStateIfChanged(Renderer&, const State&, const State&, F&&);

	static void SetGLVersion();

	impl::ShaderObject CreateShader(
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
	);

	impl::TextureObject CreateTexture(
		const std::uint8_t* pixel_data, V2_int size, TextureFormat format
	);

	void SetUniform(impl::ShaderId id, const char* uniform_name, const Matrix4& v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, float v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, V2_float v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, V3_float v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, V4_float v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, const std::vector<float>& v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, int v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, V2_int v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, V3_int v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, V4_int v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, const std::vector<int>& v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, bool v);

	/// @return The texture slot the given texture is bound to, and whether it should be pushed to
	/// batch_textures.
	std::pair<std::uint32_t, bool> GetTextureSlot(impl::TextureId tex);

	V2_int GetTextureSize(impl::TextureId id) const;
	TextureFormat GetTextureFormat(impl::TextureId id) const;

	void Destroy(impl::VertexBufferId id);
	void Destroy(impl::ElementBufferId id);
	void Destroy(impl::UniformBufferId id);
	void Destroy(impl::ShaderId id);
	void Destroy(impl::TextureId id);
	void Destroy(impl::RenderbufferId id);
	void Destroy(impl::FramebufferId id);
	void Destroy(impl::VertexArrayId id);
	void Destroy(impl::RenderTargetId id);

	impl::TextureId GetRenderTargetTexture(impl::RenderTargetId render_target) const;
	V2_int GetRenderTargetSize(impl::RenderTargetId render_target) const;
	TextureFormat GetRenderTargetTextureFormat(impl::RenderTargetId render_target) const;
	void ResizeRenderTarget(impl::RenderTargetId render_target, V2_int new_size);
	void ClearRenderTarget(impl::RenderTargetId render_target, Color color, bool set_viewport)
		const;
	void BindRenderTarget(impl::RenderTargetId render_target);
	void BindRenderPass(impl::RenderPass& render_pass);

	/// @brief Flushes the batch if adding the given number of vertices and indices would exceed
	/// batch.
	void FlushIfExceedsCapacity(std::size_t vertices, std::size_t indices);
	void FlushBatch();

	void SetViewport(Viewport viewport);
	void SetShader(impl::ShaderId shader);
	void SetViewProjection(const Matrix4& view_projection);
	void SetFramebuffer(impl::FramebufferId framebuffer);
	void SetBlend(BlendMode mode, bool enabled);
	void SetDepth(const DepthState& depth);
	void SetStencil(const StencilState& stencil);
	void SetRaster(const RasterState& raster);
	void SetScissor(const ScissorState& scissor);
	void SetColorMask(const ColorMaskState& color_mask);

	impl::TextureId GetWhiteTexture() const;

	impl::ShaderId GetShader(std::string_view name) const;

	void DrawTriangle(
		impl::ShaderId shader, const std::array<V2_float, 3>& positions, Color tint, float depth
	);

	void DrawQuad(
		impl::ShaderId shader, const std::array<V2_float, 4>& positions,
		const std::array<float, 4>& user_data, Color tint, float depth,
		const std::function<void()>& shader_setup
	);

	void DrawTexture(
		impl::ShaderId shader, impl::TextureId texture, const std::array<V2_float, 4>& positions,
		Color tint, float depth, const std::array<V2_float, 4>& tex_coords,
		const std::function<void()>& shader_setup
	);

	void DrawTexture(
		impl::ShaderId shader, impl::RenderPass& pass, impl::RenderTargetId scene_render_target,
		const std::function<void()>& shader_setup
	);

	/// @param setup Returns true if the renderer should flush the batch after adding the quad
	/// params. This allows shader uniforms to be applied to each unique quad in the batch.
	void DrawQuad(
		impl::ShaderId shader, const impl::QuadParams& p,
		const std::function<bool(impl::ShaderId, impl::QuadDesc&)>& setup
	);

	/// @return True if the given texture is currently attached to the framebuffer that is currently
	/// bound.
	bool IsTextureAttachedToCurrentFramebuffer(impl::TextureId texture) const;

	void BindScreenTarget();
	void ResizeScreenTarget(V2_int size);

	void OnEvent(EventDispatcher d);

	void BeginFrame();
	void EndFrame();

	impl::RenderPass BeginPass(impl::RenderTargetId scene_render_target);

	impl::RenderTargetObject CreateRenderTarget(V2_int size, TextureFormat format);

	impl::RenderTargetId AcquirePooledTargetCopy(impl::RenderTargetId render_target);
	impl::RenderTargetId AcquirePooledTarget(V2_int size, TextureFormat format);

	void ReleasePooledTarget(impl::RenderTargetId render_target);

	Window& window_;

	// TODO: Figure out a way to decouple event emission (specifically with the user accessible
	// SetGameSize function, which can trigger DisplayResize events) from the renderer.
	EventHandler& events_;

	std::unique_ptr<impl::gl::GLContext> gl_;

	// emit_events = false is used to prevent emitting events when initializing the window and
	// scene.
	void UpdateDisplayViewport(V2_int window_size, bool emit_events = true);

	impl::VertexBufferObject vbo_;
	impl::ElementBufferObject ebo_;
	impl::VertexArrayObject vao_;
	impl::TextureObject white_texture_;

	std::vector<impl::Vertex> batch_vertices_;
	std::vector<impl::Index> batch_indices_;
	std::vector<impl::TextureId> batch_textures_;

	Matrix4 view_projection_;

	ClearColor background_color_;
	impl::RenderTargetObject screen_target_;

	std::optional<V2_int> game_size_;
	Viewport display_viewport_;
	ScalingMode scaling_mode_{ ScalingMode::Letterbox };

	std::vector<impl::PooledTarget> rt_pool_;
	std::uint64_t pool_tick_{ 0 };
	std::size_t max_pool_size_{ 16 };
};

} // namespace ptgn