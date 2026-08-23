#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/concepts.h"
#include "renderer/draw_context.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/framebuffer_pool.h"
#include "renderer/pipeline/render_batcher.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_primitives.h"
#include "renderer/pipeline/render_request.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/vertex.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer_settings.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

class Application;
class Window;
class RenderPassBuilder;
class DrawContext;
class Stats;

namespace impl {

class ApplicationContext;
class RenderCommands;
class RendererAccessor;

inline constexpr const char* kGammaUniform{ "u_Gamma" };
inline constexpr const char* kExposureUniform{ "u_Exposure" };

namespace gl {

class GLContext;

} // namespace gl

enum class ResizeType {
	Logical,
	Display,
	Presentation
};

} // namespace impl

class Renderer {
public:
	RendererSettings GetSettings() const;
	void SetSettings(const RendererSettings& settings);

	void SetToneMappingOperator(ToneMappingOperator op);

	void SetToneMappingExposure(float exposure);

	void SetGamma(float gamma);

	void SetBackgroundColor(Color background_color);

	/// @param logical_size Setting to nullopt will dynamically use the presentation viewport size.
	void SetLogicalSize(
		std::optional<V2_int> logical_size		= std::nullopt,
		std::optional<ScalingMode> scaling_mode = ScalingMode::Letterbox
	);

	/// @param scaling_mode The method by which the logical size is scaled to fit the presentation
	/// viewport.
	void SetScalingMode(ScalingMode scaling_mode = ScalingMode::Letterbox);

	/// @param presentation_viewport Setting to nullopt will use full window size.
	/// Viewport position should be relative to the window top left.
	void SetPresentationViewport(std::optional<Viewport> presentation_viewport = std::nullopt);

	/// @return The logical size of the renderer. Returns presentation viewport size if unset.
	V2_int GetLogicalSize() const;

	/// @return The presentation viewport with position relative to the window top left.
	/// Returns a viewport covering the entire window if unset.
	Viewport GetPresentationViewport() const;

	/// @return The presentation viewport position relative to the window top left. Returns {0, 0}
	/// if unset.
	V2_int GetPresentationPosition() const;

	/// @return The presentation viewport of the renderer. If unset, returns the window size.
	V2_int GetPresentationSize() const;

	/// @brief The display viewport is the area inside presentation rectangle that the application
	/// is rendered to. It is defined by scaling the logical size to fit inside the presentation
	/// viewport according to the scaling mode. The position of the display viewport is relative to
	/// the top left of the window.
	Viewport GetDisplayViewport() const;

	/// @return The display position of the renderer.
	V2_int GetDisplayPosition() const;

	/// @return The display size of the renderer.
	V2_int GetDisplaySize() const;

	/// @return The amount by which logical size is scaled to achieve the display size.
	V2_float GetScale() const;

	/// @return The size of the entire viewport that the presentation viewport is within. This is
	/// always equal to the window size.
	V2_int GetOutputSize() const;

	void SetPrimaryWorldCamera(const std::optional<Camera>& primary_world_camera = std::nullopt);

	const std::optional<Camera>& GetPrimaryWorldCamera() const;

	impl::ShaderId GetShader(std::string_view name) const;


private:
	friend class Application;
	friend class impl::ApplicationContext;
	friend class impl::RenderPipelineManager;
	friend class impl::RenderBatcher;
	friend class impl::FramebufferPool;
	friend class impl::FramebufferObject;
	friend class impl::TextureObject;
	friend class impl::ShaderObject;
	friend class impl::RenderCommands;
	friend class impl::RendererAccessor;
	friend class Texture;
	friend class RenderPassBuilder;
	friend class DrawContext;
	template <impl::ResourceType T>
	friend class impl::Resource;

	struct DisplayResizeInfo {
		bool moved{ false };
		bool resized{ false };
		Viewport viewport{};
	};

	using EventSink = std::function<void(V2_int, impl::ResizeType)>;

	Renderer() = delete;
	explicit Renderer(Window& window, Stats& stats, EventSink&& event_sink);
	~Renderer() noexcept;
	Renderer(const Renderer&)				 = delete;
	Renderer(Renderer&&) noexcept			 = delete;
	Renderer& operator=(const Renderer&)	 = delete;
	Renderer& operator=(Renderer&&) noexcept = delete;

	impl::PipelineId GetTexturePipeline() const;

	RenderState GetRenderState() const;

	void SetRenderState(const RenderState& state);
	void SetRenderStateDelta(const RenderStateDelta& delta);

	void SetMaterial(const MaterialState& material);

	void FlushBatch();

	void DrawTexture(const impl::DrawTextureRequest& request);

	template <impl::RenderPrimitive T>
	void Draw(const impl::DrawRequest<T>& request) {
		if (request.primitives.empty()) {
			return;
		}

		if (request.effect_params.draw_callback ||
			IsAttachedToCurrentFramebuffer(request.texture)) {
			DrawWithEffect(request);
		} else {
			DrawNormally(request);
		}
	}

	impl::RenderPipeline& GetPipeline(impl::PipelineId id);

	const impl::RenderPipeline& GetPipeline(impl::PipelineId id) const;

	void SetFramebuffer(impl::FramebufferObject* framebuffer);

	template <impl::RenderPrimitive T>
	void DrawWithEffect(const impl::DrawRequest<T>& request) {
		PTGN_ASSERT(!request.primitives.empty());

		using TVertex =
			typename impl::RenderPrimitiveInfo<std::remove_cvref_t<T>>::Vertex;

		PTGN_ASSERT(
			impl::HaveUniformDepthAndEntityId(request.primitives),
			"Batched effect vertices must have uniform depth and entity ID"
		);

		const Rect bounds{ Rect::FromPoints(
			request.primitives |
			std::views::join |
			std::views::transform([](const TVertex& vertex) {
				const auto& pos{
					impl::PositionAccessor<TVertex>::Get(vertex)
				};

				return V2_float{
					pos[0],
					pos[1],
				};
			})
		) };

		const V2_float bounds_center{ bounds.GetCenter() };

		V2_float size{
			bounds.GetSize() +
			V2_float{ request.effect_params.margin * 2 }
		};

		PTGN_ASSERT(
			size.IsPositive(),
			"Effect bounds size must be positive"
		);

		size = Max(V2_int{ Ceil(size) }, V2_int{ 1, 1 });

		TextureDesc desc{
			.size = size,
			.format = impl::GetEffectTextureFormat(
				request.effect_params.hdr
			),
		};

		if (request.texture) {
			desc.params = GetParams(request.texture).value();
		}

		auto expanded_framebuffer{
			CreateFramebuffer(desc, std::nullopt)
		};

		auto* previous_framebuffer{
			current_framebuffer_
		};

		SetFramebuffer(&expanded_framebuffer);

		const auto previous_state{
			GetRenderState()
		};

		const Viewport viewport{
			.position{},
			.size{ size },
		};

		SetScissor(ScissorState{ viewport });
		SetViewport(viewport);

		// Temporary effect framebuffer is centered around zero.
		SetViewProjection(size);

		// Important: The temporary framebuffer is centered around the origin, while
		// the primitive's bounds may not be. Copy the primitives and
		// translate their bounds center to zero before rendering.
		// Do not transform request.primitives directly because DrawNormally
		// mutates vertex positions in place.
		std::vector<T> local_primitives{
			request.primitives.begin(),
			request.primitives.end()
		};

		impl::DrawRequest<T> local_request{
			.texture = request.texture,
			.transform = Transform{
				-bounds_center
			},
			.primitives = local_primitives,
		};

		DrawNormally(local_request);

		FlushBatch();

		ExecuteEffectCallbacks(
			request.effect_params.draw_callback
		);

		FlushBatch();

		SetCurrentPipeline("texture");
		SetFramebuffer(previous_framebuffer);

		SetMaterial({
			.shader = GetShader("texture"),
			.texture_slot_capacity = GetMaxTextureSlots(),
		});

		SetRenderState(previous_state);

		auto positions{
			Rect{ size }.GetLocalVertices()
		};

		const auto& first_vertex{
			request.primitives.front().front()
		};

		const auto depth{
			impl::PositionAccessor<TVertex>::Get(
				first_vertex
			)[2]
		};

		constexpr auto color_n{
			color::White.Normalized()
		};

		constexpr auto tex_coords{
			impl::GetDefaultTextureCoordinates<true>()
		};

		const auto entity_id{
			impl::EntityIdAccessor<TVertex>::Get(
				first_vertex
			)
		};

		auto local_quad{
			impl::CreateTextureQuad(
				positions,
				depth,
				color_n,
				tex_coords,
				entity_id
			)
		};

		// The temporary texture is centered at zero, but represents the
		// original primitive bounds centered at bounds_center. Transform 
		// that local center through the entity / world transform.
		const Transform composite_transform{
			request.transform.Apply(bounds_center),
			request.transform.rotation,
			request.transform.scale,
		};

		impl::DrawTextureRequest new_request{
			.texture = GetTexture(expanded_framebuffer),
			.transform = composite_transform,
			.primitives = { &local_quad, 1 },
		};

		DrawNormally(new_request);

		temp_framebuffers_.emplace_back(
			std::move(expanded_framebuffer)
		);
	}

	template <impl::RenderPrimitive T>
	void DrawNormally(const impl::DrawRequest<T>& request) {
		PTGN_ASSERT(!request.primitives.empty());
		if (!current_material_valid_) {
			return;
		}
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
				request.primitives, pipeline.vertex_capacity, pipeline.index_capacity, textures,
				current_texture_slot_capacity_
			);
		} else if constexpr (vertex_count == 3) {
			batcher_.SubmitTriangles(
				request.primitives, pipeline.vertex_capacity, pipeline.index_capacity, textures,
				current_texture_slot_capacity_
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

	BlendMode GetBlendMode() const;

	[[nodiscard]] impl::ShaderObject CreateShader(
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
	);
	[[nodiscard]] impl::TextureObject CreateTexture(
		const std::uint8_t* pixel_data, TextureDesc desc, bool restore_bind = true
	);
	[[nodiscard]] impl::FramebufferObject CreateFramebuffer(
		TextureDesc desc, std::optional<TextureDesc> other_desc
	);

	impl::TextureId GetTexture(impl::FramebufferId framebuffer) const;
	impl::RenderbufferId GetDepthRenderbuffer(impl::FramebufferId framebuffer) const;
	impl::RenderbufferId GetStencilRenderbuffer(impl::FramebufferId framebuffer) const;
	impl::RenderbufferId GetDepthStencilRenderbuffer(impl::FramebufferId framebuffer) const;

	void SetParams(impl::FramebufferId framebuffer, TextureParams params);
	void SetParams(impl::TextureId texture, TextureParams params);
	void Resize(impl::FramebufferId framebuffer, V2_int new_size);
	void Clear(impl::FramebufferId framebuffer, Color clear_color);
	void Clear(impl::FramebufferId framebuffer, Depth clear_depth);
	void Clear(impl::FramebufferId framebuffer, Stencil clear_stencil);
	void Clear(
		impl::FramebufferId framebuffer, DepthStencil clear_depth_stencil
	);
	void BindPresentationFramebuffer();

	impl::FramebufferId GetPresentationFramebuffer() const;

	std::optional<V2_int> GetSize(impl::FramebufferId framebuffer) const;
	std::optional<TextureFormat> GetFormat(impl::FramebufferId framebuffer) const;
	std::optional<TextureParams> GetParams(impl::FramebufferId framebuffer) const;
	std::optional<TextureDesc> GetDesc(impl::FramebufferId framebuffer) const;
	std::optional<V2_int> GetSize(impl::TextureId texture) const;
	std::optional<TextureFormat> GetFormat(impl::TextureId texture) const;
	std::optional<V2_int> GetSize(impl::RenderbufferId renderbuffer) const;
	std::optional<TextureFormat> GetFormat(impl::RenderbufferId renderbuffer) const;
	std::optional<TextureParams> GetParams(impl::TextureId texture) const;
	std::optional<TextureDesc> GetDesc(impl::TextureId texture) const;

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

	void SetCurrentPipeline(std::size_t id);
	void SetCurrentPipeline(std::string_view name);

	void UploadVertices(
		const impl::RenderPipeline& pipeline, std::span<const std::byte> vertices,
		std::uint32_t vertex_size
	);
	void UploadIndices(const impl::RenderPipeline& pipeline, std::span<const impl::Index> indices);
	void DrawElements(const impl::RenderPipeline& pipeline, std::size_t index_count);

	void BeginFrame();

	template <typename F>
	void EndFrame(F&& presentation_effect_callback) {
		PTGN_ASSERT(display_viewport_.size.IsPositive());

		FlushBatch();

		if constexpr (!std::same_as<std::remove_cvref_t<F>, std::nullptr_t>) {
			static_assert(
				InvocableR<F, void, DrawContext&>,
				"Incorrect presentation effect callback signature"
			);
			ApplyPresentationEffect(std::forward<F>(presentation_effect_callback));
		}

		auto op{ renderer_settings_.tone_mapping.op };

		PTGN_ASSERT(
			!impl::RequiresHDRInput(op) ||
				IsHDRFormat(GetFormat(presentation_framebuffer_).value()),
			"Presentation framebuffer format must support HDR if tone mapping is enabled"
		);

		ApplyPresentationEffect([this, op](DrawContext& ctx) {
			ctx.Pass([this, op](auto& pass) -> RenderPassHandle {
				auto gamma_and_tonemapping_shader{ impl::GetGammaAndToneMappingShader(op) };

				auto result{ pass.Apply(gamma_and_tonemapping_shader) };

				result.Uniform(impl::kGammaUniform, renderer_settings_.gamma);

				if (op == ToneMappingOperator::Exposure || op == ToneMappingOperator::ACES) {
					result.Uniform(impl::kExposureUniform, renderer_settings_.tone_mapping.exposure);
				}

				return result;
			});
		});

		SetFramebuffer(nullptr);

		if (presentation_viewport_.has_value()) {
			framebuffer_pool_.Update();
			PTGN_ASSERT(
				batcher_.IsEmpty(),
				"No indices should be left in the batcher after finishing the render frame"
			);
			return;
		}

		PTGN_ASSERT(
			GetSize(presentation_framebuffer_) == V2_int{ Ceil(display_viewport_.size) },
			"Presentation framebuffer size must match display size"
		);

		SetCurrentPipeline("texture");
		SetMaterial(
			MaterialState{
				.shader				   = GetShader("texture"),
				.texture_slot_capacity = GetMaxTextureSlots(),
			}
		);
		SetBlendMode(BlendMode::ReplaceRGBA);
		SetViewport(display_viewport_);
		SetScissor(ScissorState{ false });
		SetViewProjection(display_viewport_.size);

		constexpr auto depth{ 0.0f };
		constexpr auto tint{ color::White };
		constexpr auto tex_coords{ impl::GetDefaultTextureCoordinates<true>() };

		auto local_vertices{ Rect{ display_viewport_.size }.GetLocalVertices() };
		auto local_quad{ impl::CreateTextureQuad(
			local_vertices, depth, tint.Normalized(), tex_coords, impl::kNoEntityId
		) };

		impl::DrawTextureRequest request;
		request.primitives = { &local_quad, 1 };
		request.texture	   = GetTexture(presentation_framebuffer_);

		DrawTexture(request);

		FlushBatch();

		framebuffer_pool_.Update();
		PTGN_ASSERT(
			batcher_.IsEmpty(),
			"No indices should be left in the batcher after finishing the render frame"
		);
	}

	void SetUniformValue(impl::ShaderId id, const char* uniform_name, const UniformValue& v);

	std::size_t GetMaxTextureSlots() const;

	/// @return True if the given texture is currently attached to the framebuffer that is currently
	/// bound.
	[[nodiscard]] bool IsAttachedToCurrentFramebuffer(impl::TextureId texture) const;

	void ResizePresentationFramebuffer(V2_int size);

	void OnOutputResize(V2_int size);

	void ResetState();

	// emit_events = false is used to prevent emitting events when initializing the window and
	// scene.
	void UpdateDisplayViewport(bool emit_events = true);

	void BindTextureSlot(std::uint32_t slot, impl::TextureId texture, bool force = false);

	[[nodiscard]] DisplayResizeInfo RecalculateDisplayViewport() const;

	const impl::FramebufferObject& GetBoundFramebuffer() const;
	impl::FramebufferObject& GetBoundFramebuffer();

	void DrawRenderPass(const impl::DrawPassRequest& request);

	void CopyFramebufferRegion(
		impl::FramebufferId source, impl::FramebufferId destination, Viewport source_region,
		V2_int destination_position
	);

	void CompositeRenderPassResult(
		impl::FramebufferId color_source, impl::FramebufferId destination,
		Viewport destination_region
	);

	void SetEntityPickingEnabled(impl::FramebufferId framebuffer, bool enabled);

	[[nodiscard]] bool IsEntityPickingEnabled(impl::FramebufferId framebuffer) const;

	void ClearEntityIds(impl::FramebufferId framebuffer);

	[[nodiscard]] std::optional<std::int32_t> ReadEntityId(
		impl::FramebufferId framebuffer, V2_int pixel
	) const;

	void BindUniforms();

	impl::ShaderId GetBoundShader() const;

	void ExecuteEffectCallbacks(const std::function<void(DrawContext&)>& effect_callback);

	bool FramebufferMatches(
		impl::FramebufferId framebuffer, TextureDesc desc, std::optional<TextureDesc> other_desc
	) const;

	template <InvocableR<void, DrawContext&> F>
	void ApplyPresentationEffect(F&& function) {
		FlushBatch();

		PTGN_ASSERT(presentation_framebuffer_, "Presentation framebuffer must be valid");

		auto size{ GetSize(presentation_framebuffer_).value() };

		PTGN_ASSERT(size.IsPositive(), "Presentation framebuffer size must be valid");

		SetFramebuffer(&presentation_framebuffer_);

		Viewport viewport{
			.position = {},
			.size	  = size,
		};

		SetViewport(viewport);
		SetViewProjection(viewport.size);
		SetScissor(ScissorState{ false });
		SetBlendMode(BlendMode::ReplaceRGBA);

		DrawContext ctx{ *this };

		std::invoke(std::forward<F>(function), ctx);

		FlushBatch();
	}

	Window& window_;

	Stats& stats_;

	EventSink event_sink_;

	std::unique_ptr<impl::gl::GLContext> gl_;

	impl::FramebufferObject presentation_framebuffer_;

	std::size_t current_texture_slot_capacity_{ 1 };
	std::vector<UniformWrite> current_uniforms_;
	bool current_material_valid_{ true };
	impl::FramebufferObject* current_framebuffer_{ nullptr };

	impl::RenderBatcher batcher_;
	impl::FramebufferPool framebuffer_pool_;
	impl::RenderPipelineManager pipeline_manager_;
	std::vector<impl::FramebufferObject> temp_framebuffers_;
	impl::TextureObject white_texture_;

	RendererSettings renderer_settings_{};

	Viewport display_viewport_;
	/// @brief Flag to indicate whether the display viewport needs to be recalculated.
	bool display_viewport_dirty_{ true };

	/// @brief The viewport used for presentation (i.e. the final output to the screen). This
	/// may be different from the window if using the editor, which has a separate viewport for
	/// the presentation.
	std::optional<Viewport> presentation_viewport_;

	std::optional<Camera> primary_world_camera_;
};

namespace impl {

class RendererAccessor {
public:
	explicit RendererAccessor(Renderer& renderer);

	[[nodiscard]] TextureObject CreateTexture(const std::uint8_t* pixel_data, TextureDesc desc);

	[[nodiscard]] ShaderObject CreateShader(
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
	);

	[[nodiscard]] FramebufferObject CreateFramebuffer(
		TextureDesc desc, const std::optional<TextureDesc>& other_desc
	);

	TextureId GetPresentationTexture() const;

	std::optional<V2_int> GetSize(TextureId texture) const;

	TextureId GetTexture(FramebufferId framebuffer) const;

	void FlushBatch();

	void SetupPresentationFramebuffer();

	ShaderId GetShader(std::string_view name) const;

	void SetFramebuffer(FramebufferObject* framebuffer);

	void SetScissor(const ScissorState& scissor);

	void SetViewProjection(const Matrix4& view_projection);

	void SetViewport(Viewport viewport);

	void SetBlendMode(BlendMode blend_mode, bool force = false);

	template <impl::RenderPrimitive T>
	void Draw(const impl::DrawRequest<T>& request) {
		renderer_.Draw(request);
	}

	const FramebufferObject& GetBoundFramebuffer() const;

	void ClearEntityIds(FramebufferId framebuffer);

	impl::FramebufferId GetPresentationFramebuffer() const;

	void SetEntityPickingEnabled(impl::FramebufferId framebuffer, bool enabled);

	std::optional<std::int32_t> ReadEntityId(
		FramebufferId framebuffer, V2_int pixel
	) const;

	bool IsEntityPickingEnabled(FramebufferId framebuffer) const;

	std::size_t GetMaxTextureSlots() const;

private:
	Renderer& renderer_;
};

} // namespace impl

} // namespace ptgn
