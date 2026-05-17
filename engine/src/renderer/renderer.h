#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/render_batch.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

class Application;
class RenderContext;
class DrawContext;
class DebugContext;
class Window;
class Scene;
class AssetManager;
class RenderTarget;

namespace impl {

struct RenderPacket;
class RenderPipelineManager;
class ApplicationContext;
class Surface;
class Renderer;
class ShaderObject;
class TextureObject;
template <ResourceType T>
class Resource;

namespace gl {

class GLContext;

} // namespace gl

inline constexpr std::uint32_t kBatchCapacity{ 10000 };
inline constexpr std::uint32_t kVertexCapacity{ kBatchCapacity * 4 };
inline constexpr std::uint32_t kIndexCapacity{ kBatchCapacity * 6 };

struct EffectParams {
	std::function<void(DrawContext&)> draw_callback;

	int margin{ 0 };
};

class Renderer {
public:
	void SetViewProjection(const Matrix4& view_projection);
	void SetFramebuffer(FramebufferId framebuffer);
	void SetViewport(Viewport viewport);
	void SetBlendMode(BlendMode blend_mode);
	void SetShader(ShaderId shader);
	void SetDepthTesting(bool enabled);
	void SetDepthMask(const DepthMaskState& mask);
	void SetStencil(const StencilState& stencil);
	void SetRaster(const RasterState& raster);
	void SetScissor(const ScissorState& scissor);
	void SetColorMask(const ColorMaskState& color_mask);

	[[nodiscard]] ShaderObject CreateShader(
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
	);
	[[nodiscard]] TextureObject CreateTexture(
		const Surface& surface, TextureFormat format, TextureParameters params = {}
	);
	[[nodiscard]] TextureObject CreateTexture(
		const std::uint8_t* pixel_data, V2_int size, TextureFormat format,
		TextureParameters params = {}
	);
	[[nodiscard]] RenderTargetObject CreateRenderTarget(const RenderTargetDesc& desc);

	ShaderId GetShader(std::string_view name) const;

	void SetGameSize(
		std::optional<V2_int> game_size			= std::nullopt,
		std::optional<ScalingMode> scaling_mode = ScalingMode::Letterbox
	);

	void SetScalingMode(ScalingMode scaling_mode = ScalingMode::Letterbox);

	void SetPresentationViewport(std::optional<Viewport> presentation_viewport = std::nullopt);

	[[nodiscard]] bool HasGameSize() const;

	V2_int GetGameSize() const;

	ScalingMode GetScalingMode() const;

	Viewport GetPresentationViewport() const;

	V2_int GetPresentationPosition() const;

	V2_int GetPresentationSize() const;

	Viewport GetDisplayViewport() const;

	V2_int GetDisplayPosition() const;

	V2_int GetDisplaySize() const;

	V2_float GetScale() const;

	V2_int GetFullViewportSize() const;

	void SetBackgroundColor(Color background_color);
	Color GetBackgroundColor() const;

	void SetPrimaryWorldCamera(const std::optional<Camera>& primary_world_camera = std::nullopt);

	const std::optional<Camera>& GetPrimaryWorldCamera() const;

	TextureId GetRenderTargetTexture(RenderTargetId render_target) const;

	V2_int GetRenderTargetSize(RenderTargetId render_target) const;
	TextureFormat GetRenderTargetTextureFormat(RenderTargetId render_target) const;
	void ResizeRenderTarget(RenderTargetId render_target, V2_int new_size);
	void ClearRenderTarget(RenderTargetId render_target, Color color, bool set_viewport) const;
	void BindRenderTarget(RenderTargetId render_target);
	void BindScreenTarget();

	RenderTargetId GetScreenTarget() const;

	V2_int GetTextureSize(TextureId id) const;
	TextureFormat GetTextureFormat(TextureId id) const;

	void SetUniform(ShaderId id, const char* uniform_name, const Matrix4& v);
	void SetUniform(ShaderId id, const char* uniform_name, float v);
	void SetUniform(ShaderId id, const char* uniform_name, V2_float v);
	void SetUniform(ShaderId id, const char* uniform_name, V3_float v);
	void SetUniform(ShaderId id, const char* uniform_name, V4_float v);
	void SetUniform(ShaderId id, const char* uniform_name, std::span<const float> v);
	void SetUniform(ShaderId id, const char* uniform_name, int v);
	void SetUniform(ShaderId id, const char* uniform_name, V2_int v);
	void SetUniform(ShaderId id, const char* uniform_name, V3_int v);
	void SetUniform(ShaderId id, const char* uniform_name, V4_int v);
	void SetUniform(ShaderId id, const char* uniform_name, std::span<const int> v);
	void SetUniform(ShaderId id, const char* uniform_name, bool v);

	void Destroy(VertexBufferId id);
	void Destroy(ElementBufferId id);
	void Destroy(UniformBufferId id);
	void Destroy(ShaderId id);
	void Destroy(TextureId id);
	void Destroy(RenderbufferId id);
	void Destroy(FramebufferId id);
	void Destroy(VertexArrayId id);
	void Destroy(RenderTargetId id);

private:
	friend class ptgn::Application;
	friend class ApplicationContext;

	struct DisplayResizeInfo {
		bool moved{ false };
		bool resized{ false };
		Viewport viewport;
	};

	using EventSink =
		std::function<void(V2_int, std::variant<ResizeType, impl::PresentationResizeType>)>;

	Renderer() = delete;
	explicit Renderer(Window& window, EventSink&& event_sink);
	~Renderer() noexcept;
	Renderer(const Renderer&)				 = delete;
	Renderer(Renderer&&) noexcept			 = delete;
	Renderer& operator=(const Renderer&)	 = delete;
	Renderer& operator=(Renderer&&) noexcept = delete;

	void FlushBatch();

	void UploadVertices(const RenderPipeline& pipeline, std::span<const std::byte> vertices);
	void UploadIndices(const RenderPipeline& pipeline, std::span<const Index> indices);
	void DrawElements(const RenderPipeline& pipeline, std::uint32_t index_count);

	void ApplyMaterial(const MaterialState& material);

	void BeginFrame();
	void EndFrame();

	void SetCurrentPipeline(std::size_t id);
	void SetCurrentPipeline(std::string_view name);

	void SetUniformValue(ShaderId id, const char* uniform_name, const UniformValue& v);

	[[nodiscard]] bool IsPresentationViewportVisible() const;

	std::size_t GetMaxTextureSlots() const;

	/// @return True if the given texture is currently attached to the framebuffer that is currently
	/// bound.
	[[nodiscard]] bool IsTextureAttachedToCurrentFramebuffer(TextureId texture) const;

	void ResizeScreenTarget(V2_int size);

	void OnWindowResize(V2_int size);

	void InvalidateState();

	// emit_events = false is used to prevent emitting events when initializing the window and
	// scene.
	void UpdateDisplayViewport(bool emit_events = true);

	[[nodiscard]] DisplayResizeInfo RecalculateDisplayViewport() const;

	Window& window_;

	EventSink event_sink_;

	std::unique_ptr<gl::GLContext> gl_;

	/// @brief Currently set view projection.
	Matrix4 view_projection_;

	Color background_color_;
	RenderTargetObject screen_target_;

	Batch batch_;
	RenderTargetPool render_target_pool_;
	RenderPipelineManager pipeline_manager_;

	std::optional<V2_int> game_size_;
	Viewport display_viewport_;
	ScalingMode scaling_mode_{ ScalingMode::Letterbox };

	/// @brief The viewport used for presentation (i.e. the final output to the screen). This
	/// may be different from the window if using the editor, which has a separate viewport for
	/// the game view.
	std::optional<Viewport> presentation_viewport_;

	/// @brief Flag to indicate whether the display viewport needs to be recalculated.
	bool display_viewport_dirty_{ true };

	std::optional<Camera> primary_world_camera_;
};

} // namespace impl

} // namespace ptgn