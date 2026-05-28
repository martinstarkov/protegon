#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/render_batcher.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/vertex.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

class Application;
class Window;
class PassBuilder;
class DrawContext;
class Stats;

namespace impl {

class ApplicationContext;
class RenderCommands;
class RendererAccessor;

namespace gl {

class GLContext;

} // namespace gl

void ApplyTransform(Transform transform, std::span<TextureQuad> local_quads);

struct DrawTextureRequest {
	TextureId texture;
	/// @brief Center of the texture in world space. Origin should be accounted for in this
	/// transform.
	Transform transform;
	std::span<TextureQuad> local_quads;
	EffectParams effect_params;
};

} // namespace impl

class Renderer {
public:
	/// @param game_size Setting to nullopt will dynamically use the presentation viewport size.
	void SetGameSize(
		std::optional<V2_int> game_size			= std::nullopt,
		std::optional<ScalingMode> scaling_mode = ScalingMode::Letterbox
	);

	/// @param scaling_mode The method by which the game size is scaled to fit the presentation
	/// viewport.
	void SetScalingMode(ScalingMode scaling_mode = ScalingMode::Letterbox);

	/// @param presentation_viewport Setting to nullopt will use full window size.
	/// Viewport position should be relative to the window top left.
	void SetPresentationViewport(std::optional<Viewport> presentation_viewport = std::nullopt);

	/// @return True if the game size is set, false otherwise.
	[[nodiscard]] bool HasGameSize() const;

	/// @return The game size of the renderer. Returns presentation viewport size if unset.
	V2_int GetGameSize() const;

	/// @return The method by which the game size is scaled to fit the presentation
	/// viewport.
	ScalingMode GetScalingMode() const;

	/// @return The presentation viewport with position relative to the window top left.
	/// Returns a viewport covering the entire window if unset.
	Viewport GetPresentationViewport() const;

	/// @return The presentation viewport position relative to the window top left. Returns {0, 0}
	/// if unset.
	V2_int GetPresentationPosition() const;

	/// @return The presentation viewport of the renderer. If unset, returns the window size.
	V2_int GetPresentationSize() const;

	/// @brief The display viewport is the area inside presentation rectangle that the game is
	/// rendered to. It is defined by scaling the game size to fit inside the presentation viewport
	/// according to the scaling mode.
	/// The position of the display viewport is relative to the top left of the window.
	Viewport GetDisplayViewport() const;

	/// @return The display position of the renderer.
	V2_int GetDisplayPosition() const;

	/// @return The display size of the renderer.
	V2_int GetDisplaySize() const;

	/// @return The amount by which game size is scaled to achieve the display size.
	V2_float GetScale() const;

	/// @return The size of the entire viewport that the presentation viewport is within. This is
	/// always equal to the window size.
	V2_int GetFullViewportSize() const;

	void SetBackgroundColor(Color background_color);
	Color GetBackgroundColor() const;

	void SetPrimaryWorldCamera(const std::optional<Camera>& primary_world_camera = std::nullopt);

	const std::optional<Camera>& GetPrimaryWorldCamera() const;

private:
	friend class Application;
	friend class impl::ApplicationContext;
	friend class impl::RenderPipelineManager;
	friend class impl::RenderBatcher;
	friend class impl::RenderTargetPool;
	friend class impl::RenderTargetObject;
	friend class impl::TextureObject;
	friend class impl::ShaderObject;
	friend class impl::RenderCommands;
	friend class impl::RendererAccessor;
	friend class PassBuilder;
	friend class DrawContext;
	template <impl::ResourceType T>
	friend class impl::Resource;

	struct DisplayResizeInfo {
		bool moved{ false };
		bool resized{ false };
		Viewport viewport;
	};

	using EventSink =
		std::function<void(V2_int, std::variant<ResizeType, impl::PresentationResizeType>)>;

	Renderer() = delete;
	explicit Renderer(Window& window, Stats& stats, EventSink&& event_sink);
	~Renderer() noexcept;
	Renderer(const Renderer&)				 = delete;
	Renderer(Renderer&&) noexcept			 = delete;
	Renderer& operator=(const Renderer&)	 = delete;
	Renderer& operator=(Renderer&&) noexcept = delete;

	impl::PipelineId GetTexturePipeline() const;

	RenderState GetRenderState() const;

	void SetRenderState(const RenderState& render_state);

	void SetMaterial(const MaterialState& material);

	void FlushBatch();

	void DrawTexture(const impl::DrawTextureRequest& request);

	impl::RenderPipeline& GetPipeline(impl::PipelineId id);

	const impl::RenderPipeline& GetPipeline(impl::PipelineId id) const;

	template <impl::VertexType TVertex>
	void DrawQuads(
		std::span<impl::RenderQuad<TVertex>> quads, std::span<const impl::TextureId> textures = {}
	) {
		if (quads.empty()) {
			return;
		}

		const auto& pipeline{ pipeline_manager_.GetCurrentPipeline() };

		batcher_.SubmitQuads(quads, pipeline.vertex_capacity, pipeline.index_capacity, textures);
	}

	template <impl::VertexType TVertex>
	void DrawTriangles(
		std::span<impl::RenderTriangle<TVertex>> triangles,
		std::span<const impl::TextureId> textures = {}
	) {
		if (triangles.empty()) {
			return;
		}

		const auto& pipeline{ pipeline_manager_.GetCurrentPipeline() };

		batcher_.SubmitTriangles(
			triangles, pipeline.vertex_capacity, pipeline.index_capacity, textures
		);
	}

	template <impl::RenderPrimitive TPrimitive>
	void Draw(std::span<TPrimitive> primitives, std::span<const impl::TextureId> textures = {}) {
		using Info = impl::RenderPrimitiveInfo<TPrimitive>;

		if constexpr (Info::vertex_count == 3) {
			DrawTriangles(primitives, textures);
		} else if constexpr (Info::vertex_count == 4) {
			DrawQuads(primitives, textures);
		} else {
			static_assert(
				false, "Cannot use Draw for primitives other than RenderQuad and RenderTriangle"
			);
		}
	}

	template <impl::RenderPrimitive TPrimitive>
	void Draw(std::span<TPrimitive> primitives, impl::TextureId texture = {}) {
		std::span<const impl::TextureId> textures;

		if (texture) {
			textures = { &texture, 1 };
		}

		Draw(primitives, textures);
	}

	void SetRenderTarget(impl::RenderTargetObject* target);
	void UpdateRenderTarget(impl::RenderTargetObject&& replacing_target);

	void DrawTextureEffect(const impl::DrawTextureRequest& request);
	void DrawTextureNormally(const impl::DrawTextureRequest& request);

	/// @brief Set the view projection to an orthographic projection matrix with the given size,
	/// centered at the origin.
	void SetViewProjection(V2_float size);
	void SetViewProjection(const Matrix4& view_projection);
	void SetViewport(Viewport viewport);
	void SetBlendMode(BlendMode blend_mode);
	void SetBlending(bool enabled);
	void SetShader(impl::ShaderId shader);
	void SetShader(std::string_view shader);
	void SetDepthTesting(bool enabled);
	void SetDepthMask(const DepthMaskState& mask);
	void SetStencil(const StencilState& stencil);
	void SetRaster(const RasterState& raster);
	void SetScissor(const ScissorState& scissor);
	void SetColorMask(const ColorMaskState& color_mask);

	std::optional<BlendMode> GetBlendMode() const;

	[[nodiscard]] impl::ShaderObject CreateShader(
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
	);
	[[nodiscard]] impl::TextureObject CreateTexture(
		const std::uint8_t* pixel_data, V2_int size, TextureFormat format, TextureParams params
	);
	[[nodiscard]] impl::RenderTargetObject CreateRenderTarget(const RenderTargetDesc& desc);

	impl::ShaderId GetShader(std::string_view name) const;

	impl::TextureId GetRenderTargetTexture(impl::RenderTargetId render_target) const;

	V2_int GetRenderTargetSize(impl::RenderTargetId render_target) const;
	TextureFormat GetRenderTargetTextureFormat(impl::RenderTargetId render_target) const;
	TextureParams GetRenderTargetTextureParams(impl::RenderTargetId render_target) const;
	void SetTextureParams(impl::TextureId texture, TextureParams params);
	void ResizeRenderTarget(impl::RenderTargetId render_target, V2_int new_size);
	void ClearRenderTarget(
		impl::RenderTargetId render_target, Color color, bool set_viewport, bool restore_bind
	) const;
	void BindPresentationTarget();

	impl::RenderTargetId GetPresentationTarget() const;

	V2_int GetTextureSize(impl::TextureId id) const;
	TextureFormat GetTextureFormat(impl::TextureId id) const;
	TextureParams GetTextureParams(impl::TextureId texture) const;

	/// @brief For binding textures to shader uniforms.
	void SetBoundShaderUniform(const char* uniform_name, int value);
	void SetUniform(impl::ShaderId id, const char* uniform_name, const Matrix4& v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, float v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, V2_float v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, V3_float v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, V4_float v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, std::span<const float> v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, int v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, V2_int v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, V3_int v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, V4_int v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, std::span<const int> v);
	void SetUniform(impl::ShaderId id, const char* uniform_name, bool v);

	void Destroy(impl::VertexBufferId id);
	void Destroy(impl::ElementBufferId id);
	void Destroy(impl::UniformBufferId id);
	void Destroy(impl::ShaderId id);
	void Destroy(impl::TextureId id);
	void Destroy(impl::RenderbufferId id);
	void Destroy(impl::FramebufferId id);
	void Destroy(impl::VertexArrayId id);
	void Destroy(impl::RenderTargetId id);

	void SetCurrentPipeline(std::size_t id);
	void SetCurrentPipeline(std::string_view name);

	void UploadVertices(
		const impl::RenderPipeline& pipeline, std::span<const std::byte> vertices,
		std::uint32_t vertex_size
	);
	void UploadIndices(const impl::RenderPipeline& pipeline, std::span<const impl::Index> indices);
	void DrawElements(const impl::RenderPipeline& pipeline, std::uint32_t index_count);

	void BeginFrame();
	void EndFrame();

	void SetUniformValue(impl::ShaderId id, const char* uniform_name, const UniformValue& v);

	[[nodiscard]] bool IsPresentationViewportVisible() const;

	std::size_t GetMaxTextureSlots() const;

	/// @return True if the given texture is currently attached to the framebuffer that is currently
	/// bound.
	[[nodiscard]] bool IsTextureAttachedToCurrentFramebuffer(impl::TextureId texture) const;

	void ResizePresentationTarget(V2_int size);

	void OnWindowResize(V2_int size);

	void InvalidateState();

	// emit_events = false is used to prevent emitting events when initializing the window and
	// scene.
	void UpdateDisplayViewport(bool emit_events = true);

	void BindTextureSlot(std::uint32_t slot, impl::TextureId texture);

	[[nodiscard]] DisplayResizeInfo RecalculateDisplayViewport() const;

	const impl::RenderTargetObject& GetRenderTarget() const;

	void DrawRenderPass(
		impl::ShaderId shader, std::size_t pipeline, std::span<const impl::BoundInput> inputs,
		impl::RenderTargetId output
	);

	Window& window_;

	Stats& stats_;

	EventSink event_sink_;

	std::unique_ptr<impl::gl::GLContext> gl_;

	Color background_color_;
	impl::RenderTargetObject presentation_target_;

	std::vector<UniformWrite> current_uniforms_;
	impl::RenderTargetObject* current_target_{ nullptr };

	impl::RenderBatcher batcher_;
	impl::RenderTargetPool target_pool_;
	impl::RenderPipelineManager pipeline_manager_;
	std::vector<impl::RenderTargetObject> temp_render_targets_;

	std::optional<V2_int> game_size_;
	Viewport display_viewport_;
	/// @brief Flag to indicate whether the display viewport needs to be recalculated.
	bool display_viewport_dirty_{ true };
	ScalingMode scaling_mode_{ ScalingMode::Letterbox };

	/// @brief The viewport used for presentation (i.e. the final output to the screen). This
	/// may be different from the window if using the editor, which has a separate viewport for
	/// the game view.
	std::optional<Viewport> presentation_viewport_;

	std::optional<Camera> primary_world_camera_;
};

namespace impl {

class RendererAccessor {
public:
	explicit RendererAccessor(Renderer& renderer);

	[[nodiscard]] TextureObject CreateTexture(
		const std::uint8_t* pixel_data, V2_int size, TextureFormat format, TextureParams params
	);

	[[nodiscard]] ShaderObject CreateShader(
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
	);

	[[nodiscard]] RenderTargetObject CreateRenderTarget(const RenderTargetDesc& desc);

	TextureId GetPresentationTexture() const;

	void FlushBatch();

	void SetupPresentationTarget();

	ShaderId GetShader(std::string_view name) const;

	void SetRenderTarget(RenderTargetObject* target);

	void SetScissor(const ScissorState& scissor);

	void SetViewProjection(const Matrix4& view_projection);

	void SetViewport(Viewport viewport);

	template <RenderPrimitive TPrimitive>
	void Draw(std::span<TPrimitive> primitives, TextureId texture = {}) {
		renderer_.Draw(primitives, texture);
	}

private:
	Renderer& renderer_;
};

} // namespace impl

} // namespace ptgn