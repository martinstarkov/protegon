#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
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

template <RenderPrimitive T>
void ApplyTransform(Transform transform, std::span<T> primitives) {
	using TVertex = typename RenderPrimitiveInfo<std::remove_cvref_t<T>>::Vertex;

	transform.ApplyTo(
		primitives | std::views::join,
		[](const TVertex& vertex) {
			const auto& pos{ PositionAccessor<TVertex>::Get(vertex) };
			return V2_float{ pos[0], pos[1] };
		},
		[](TVertex& vertex, V2_float position) {
			auto& pos{ PositionAccessor<TVertex>::Get(vertex) };
			pos[0] = position.x;
			pos[1] = position.y;
		}
	);
}

/// @return True if all vertices in all primitives have the same depth and entity ID, false
/// otherwise.
template <RenderPrimitive T>
bool HaveUniformDepthAndEntityId(std::span<T> primitives) {
	PTGN_ASSERT(!primitives.empty());

	const auto& first_primitive{ primitives.front() };
	const auto& first_vertex{ first_primitive.front() };

	using TVertex = typename RenderPrimitiveInfo<std::remove_cvref_t<T>>::Vertex;

	auto get_depth = [](const auto& vertex) -> float {
		return impl::PositionAccessor<TVertex>::Get(vertex)[2];
	};

	auto get_entity_id = [](const auto& vertex) -> int {
		return impl::EntityIdAccessor<TVertex>::Get(vertex);
	};

	auto first_depth{ get_depth(first_vertex) };
	auto first_entity_id{ get_entity_id(first_vertex) };

	for (const auto& primitive : primitives) {
		for (const auto& vertex : primitive) {
			if (!NearlyEqual(get_depth(vertex), first_depth) ||
				get_entity_id(vertex) != first_entity_id) {
				return false;
			}
		}
	}

	return true;
}

template <RenderPrimitive T>
struct DrawRequest {
	/// @brief Optional texture to apply to the primitive. If the pipeline does not support
	/// texturing, this field will be ignored.
	TextureId texture;
	/// @brief Center of the primitive in world space. Origin should be accounted for in this
	/// transform.
	Transform transform;

	std::span<T> primitives;

	EffectParams effect_params;
};

using DrawTextureRequest = DrawRequest<TextureQuad>;

template <VertexType TVertex>
using DrawQuadsRequest = DrawRequest<RenderQuad<TVertex>>;

template <VertexType TVertex>
using DrawTrianglesRequest = DrawRequest<RenderTriangle<TVertex>>;

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

	template <impl::RenderPrimitive T>
	void Draw(const impl::DrawRequest<T>& request) {
		if (request.primitives.empty()) {
			return;
		}

		if (request.effect_params.draw_callback ||
			IsTextureAttachedToCurrentFramebuffer(request.texture)) {
			DrawWithEffect(request);
		} else {
			DrawNormally(request);
		}
	}

	impl::RenderPipeline& GetPipeline(impl::PipelineId id);

	const impl::RenderPipeline& GetPipeline(impl::PipelineId id) const;

	void SetRenderTarget(impl::RenderTargetObject* target);
	void UpdateRenderTarget(impl::RenderTargetObject&& replacing_target);

	template <impl::RenderPrimitive T>
	void DrawWithEffect(const impl::DrawRequest<T>& request) {
		PTGN_ASSERT(!request.primitives.empty());

		using TVertex = typename impl::RenderPrimitiveInfo<std::remove_cvref_t<T>>::Vertex;

		PTGN_ASSERT(
			impl::HaveUniformDepthAndEntityId(request.primitives),
			"Batched effect vertices must have uniform depth and entity ID"
		);

		auto bounds{ Rect::FromPoints(
			request.primitives | std::views::join |
			std::views::transform([](const TVertex& vertex) {
				const auto& pos{ impl::PositionAccessor<TVertex>::Get(vertex) };
				return V2_float{ pos[0], pos[1] };
			})
		) };

		PTGN_ASSERT(bounds.HasPositiveArea());

		V2_float size{ bounds.GetSize() };

		PTGN_ASSERT(size.IsPositive());

		size += V2_float{ request.effect_params.margin * 2 };

		RenderTargetDesc desc{ .size = size };

		if (request.texture) {
			desc.format = GetTextureFormat(request.texture);
			desc.params = GetTextureParams(request.texture);
		}

		auto expanded_target{ CreateRenderTarget(desc) };

		PTGN_ASSERT(expanded_target.GetSize() == V2_int{ size });

		auto previous_target{ current_target_ };

		SetRenderTarget(&expanded_target);

		auto previous_state{ GetRenderState() };

		Viewport viewport{ .position{}, .size{ size } };
		SetScissor(ScissorState{ viewport });
		SetViewport(viewport);
		SetViewProjection(size);

		impl::DrawRequest<T> local_request;

		local_request.primitives = request.primitives;
		local_request.texture	 = request.texture;

		DrawNormally(local_request);

		FlushBatch();

		ExecuteEffectCallbacks(request.effect_params.draw_callback);

		FlushBatch();

		SetCurrentPipeline("texture");
		SetRenderTarget(previous_target);
		SetShader("texture");
		SetRenderState(previous_state);

		auto positions{ Rect{ size }.GetLocalVertices() };

		PTGN_ASSERT(!request.primitives.empty());

		const auto& first_primitive{ request.primitives.front() };

		PTGN_ASSERT(!first_primitive.empty());

		const auto& first_vertex{ first_primitive.front() };

		auto depth{ impl::PositionAccessor<TVertex>::Get(first_vertex)[2] };

		constexpr auto color_n{ color::White.Normalized() };

		constexpr auto tex_coords{ impl::GetDefaultTextureCoordinates<true>() };

		auto entity_id{ impl::EntityIdAccessor<TVertex>::Get(first_vertex) };

		auto local_quad{
			impl::CreateTextureQuad(positions, depth, color_n, tex_coords, entity_id)
		};

		impl::DrawTextureRequest new_request{ .texture	  = GetRenderTargetTexture(expanded_target),
											  .transform  = request.transform,
											  .primitives = { &local_quad, 1 } };

		DrawNormally(new_request);

		temp_render_targets_.emplace_back(std::move(expanded_target));
	}

	template <impl::RenderPrimitive T>
	void DrawNormally(const impl::DrawRequest<T>& request) {
		PTGN_ASSERT(!request.primitives.empty());
		PTGN_ASSERT(!request.effect_params.draw_callback);

		std::span<const impl::TextureId> textures;

		if (request.texture) {
			textures = { &request.texture, 1 };
		}

		impl::ApplyTransform(request.transform, request.primitives);

		const auto& pipeline{ pipeline_manager_.GetCurrentPipeline() };

		constexpr auto vertex_count{
			impl::RenderPrimitiveInfo<std::remove_cvref_t<T>>::vertex_count
		};

		if constexpr (vertex_count == 4) {
			batcher_.SubmitQuads(
				request.primitives, pipeline.vertex_capacity, pipeline.index_capacity, textures
			);
		} else if constexpr (vertex_count == 3) {
			batcher_.SubmitTriangles(
				request.primitives, pipeline.vertex_capacity, pipeline.index_capacity, textures
			);
		} else {
			static_assert(false, "Unsupported vertex count");
		}
	}

	/// @brief Set the view projection to an orthographic projection matrix with the given size,
	/// centered at the origin.
	void SetViewProjection(V2_float size);

	void SetViewProjection(const Matrix4& view_projection);
	void SetViewport(Viewport viewport);
	/// @param force If true, will set the blend mode even if it is the same as the current blend
	/// mode. This is useful for resetting blend mode state that may have been changed by external
	/// code.
	void SetBlendMode(BlendMode blend_mode, bool force = false);
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
	void EndFrame(const std::function<void(DrawContext&)>& screen_effect_callback);

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

	const impl::RenderTargetObject& GetBoundRenderTarget() const;

	void DrawRenderPass(const impl::DrawPassRequest& request);

	void CopyRenderTargetRegion(
		impl::RenderTargetId source, impl::RenderTargetId destination, Viewport source_region,
		V2_int destination_position
	);

	void CompositeRenderPassResult(
		impl::RenderTargetId source, impl::RenderTargetId destination, Viewport destination_region
	);

	void ApplyScreenEffects(const std::function<void(DrawContext&)>& screen_effect_callback);

	void BindUniforms();

	std::optional<impl::ShaderId> GetBoundShader() const;

	void ExecuteEffectCallbacks(const std::function<void(DrawContext&)>& effect_callback);

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

	void SetBlendMode(BlendMode blend_mode, bool force = false);

	template <impl::RenderPrimitive T>
	void Draw(const impl::DrawRequest<T>& request) {
		renderer_.Draw(request);
	}

	const RenderTargetObject& GetBoundRenderTarget() const;

private:
	Renderer& renderer_;
};

} // namespace impl

} // namespace ptgn