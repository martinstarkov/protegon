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
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/hash.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/render_batcher.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

class Application;
class DrawContext;
class Window;

namespace impl {

class ApplicationContext;
class Surface;
class RenderPipelineManager;

namespace gl {

class GLContext;

} // namespace gl

struct EffectParams {
	std::function<void(DrawContext&)> draw_callback;

	int margin{ 0 };
};

class Renderer {
public:
	void SetMaterial(const MaterialState& material);

	void BeginScene(RenderTargetObject& scene_target, Color clear_color);

	void EndScene();

	void FlushBatch();

	RenderTargetPool& GetTargetPool();

	RenderPipeline& GetPipeline(PipelineId id);

	const RenderPipeline& GetPipeline(PipelineId id) const;

	template <VertexType TVertex, typename TAccessor = DefaultTextureIndexAccessor<TVertex>>
	void DrawQuads(
		std::span<const RenderQuad<TVertex>> quads, std::span<const TextureId> local_textures = {},
		TAccessor texture_index = {}
	) {
		PTGN_ASSERT(pipeline_manager_.GetCurrentPipelineId() != 0);

		RenderState current_state{ GetCurrentState() };

		batcher_.SubmitQuads<TVertex>(
			pipeline_manager_.GetCurrentPipelineId(), GetCurrentTarget(), GetCurrentMaterial(),
			current_state, quads, local_textures, texture_index
		);
	}

	template <VertexType TVertex, typename TAccessor = DefaultTextureIndexAccessor<TVertex>>
	void DrawTriangles(
		std::span<const RenderTriangle<TVertex>> triangles,
		std::span<const TextureId> local_textures = {}, TAccessor texture_index = {}
	) {
		PTGN_ASSERT(pipeline_manager_.GetCurrentPipelineId() != 0);

		RenderState current_state{ GetCurrentState() };

		batcher_.SubmitTriangles<TVertex>(
			pipeline_manager_.GetCurrentPipelineId(), GetCurrentTarget(), GetCurrentMaterial(),
			current_state, triangles, local_textures, texture_index
		);
	}

	void DrawTexture(
		const MaterialState& material, TextureSource texture,
		const std::array<V2_float, 4>& positions, float depth, Color tint,
		const std::array<V2_float, 4>& tex_coords, const EffectParams& effects = {},
		std::span<const TextureBinding> extra_textures = {}, int entity_id = -1
	);

	RenderPassBuilder Pass();

	TextureSource DrawPass(
		const MaterialState& material, TextureSource input, const RenderTargetDesc& output_desc,
		const RenderState& state, std::span<const TextureBinding> extra_textures
	);

private:
	RenderState GetCurrentState() const;
	FramebufferId GetCurrentTarget() const;
	MaterialState GetCurrentMaterial() const;

	struct TargetSave {
		FramebufferId target{ 0 };
		bool transient{ false };
	};

	TargetSave SaveTarget() const;

	void RestoreTarget(TargetSave save);

	void DrawBoundTargetEffect(
		const std::array<V2_float, 4>& positions, float depth, Color tint,
		const std::array<V2_float, 4>& tex_coords, std::span<const TextureBinding> extra_textures
	);

	void DrawTextureWithEffects(
		TextureSource source, const std::array<V2_float, 4>& world_positions, float depth,
		Color tint, const std::array<V2_float, 4>& tex_coords, const EffectParams& effects,
		int entity_id
	);

	void DrawImmediateTexturedQuad(
		TextureSource primary, const std::array<V2_float, 4>& positions, float depth, Color tint,
		const std::array<V2_float, 4>& tex_coords, std::span<const TextureBinding> extra_textures
	);

	TextureId ResolveTexture(TextureSource source) const;

	FramebufferId ResolveTarget(TextureSource source) const;

	RenderTargetDesc GetTextureDesc(TextureSource source) const;

	static std::array<V2_float, 4> FullscreenQuad(V2_int size);

	static std::array<V2_float, 4> QuadInsidePaddedTarget(V2_int source_size);

	static std::array<V2_float, 4> ExpandQuadByPixels(
		std::array<V2_float, 4> quad, V2_int source_size, int margin
	);

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

	void SetCurrentPipeline(std::size_t id);
	void SetCurrentPipeline(std::string_view name);

private:
	friend class ptgn::Application;
	friend class ApplicationContext;
	friend class RenderPipelineManager;
	friend class RenderBatcher;

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

	void UploadVertices(const RenderPipeline& pipeline, std::span<const std::byte> vertices);
	void UploadIndices(const RenderPipeline& pipeline, std::span<const Index> indices);
	void DrawElements(const RenderPipeline& pipeline, std::uint32_t index_count);

	void ApplyRenderTarget(FramebufferId id);

	void ApplyRenderState(const RenderState& state);

	void ApplyMaterial(const MaterialState& material);

	void BeginFrame();
	void EndFrame();

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

	void BindTextureSlot(std::uint32_t slot, TextureId texture);

	[[nodiscard]] DisplayResizeInfo RecalculateDisplayViewport() const;

	Window& window_;

	EventSink event_sink_;

	std::unique_ptr<gl::GLContext> gl_;

	/// @brief Currently set view projection.
	Matrix4 view_projection_;

	Color background_color_;
	RenderTargetObject screen_target_;

	std::vector<UniformWrite> current_uniforms_;
	bool current_target_is_transient_{ false };

	RenderBatcher batcher_;
	RenderTargetPool target_pool_;
	RenderPipelineManager pipeline_manager_;

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

} // namespace impl

} // namespace ptgn