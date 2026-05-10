#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "core/util/concepts.h"
#include "core/util/hash.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/primitive_mode.h"
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

namespace impl {

class ApplicationContext;
class Surface;
class Renderer;
class ShaderObject;
class TextureObject;
template <ResourceType T>
class Resource;
template <typename State, InvocableR<void> F>
void UpdateStateIfChanged(Renderer&, const std::optional<State>&, const State&, F&&);

namespace gl {

class GLContext;

} // namespace gl

using Index = std::uint32_t;

inline constexpr std::uint32_t kBatchCapacity{ 10000 };
inline constexpr std::uint32_t kVertexCapacity{ kBatchCapacity * 4 };
inline constexpr std::uint32_t kIndexCapacity{ kBatchCapacity * 6 };

struct PooledTarget {
	RenderTargetObject target;
	std::uint64_t last_used_tick{ 0 };
	bool in_use{ false };
};

class Renderer {
public:
	ShaderObject CreateShader(
		const std::variant<ShaderCode, ShaderPath, ShaderPair>& source, std::string_view shader_name
	);
	TextureObject CreateTexture(
		const Surface& surface, TextureFormat format, TextureParameters params = {}
	);
	TextureObject CreateTexture(
		const std::uint8_t* pixel_data, V2_int size, TextureFormat format,
		TextureParameters params = {}
	);
	RenderTargetObject CreateRenderTarget(
		V2_int size, TextureFormat format, TextureParameters params = {}
	);

	ShaderId GetShader(std::string_view name) const;

	void SetGameSize(
		std::optional<V2_int> game_size			= std::nullopt,
		std::optional<ScalingMode> scaling_mode = ScalingMode::Letterbox
	);

	void SetScalingMode(ScalingMode scaling_mode = ScalingMode::Letterbox);

	void SetPresentationViewport(std::optional<Viewport> presentation_viewport = std::nullopt);

	bool HasGameSize() const;

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

	void FlushBatch();

	TextureId GetRenderTargetTexture(RenderTargetId render_target) const;

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

	void DrawTriangle(
		ShaderId shader, const std::array<V2_float, 3>& positions, float depth, Color tint,
		int entity_id
	);

	void DrawQuad(
		ShaderId shader, const std::array<V2_float, 4>& positions, float depth, Color tint,
		int entity_id
	);

	void DrawShape(
		ShaderId shader, const std::array<V2_float, 4>& positions, float depth, Color tint,
		const std::array<V2_float, 4>& tex_coords, const std::array<float, 4>& shape_data,
		int entity_id
	);

	void DrawShader(
		ShaderId shader, const std::array<V2_float, 4>& positions, float depth, Color tint,
		const std::array<V2_float, 4>& tex_coords, const std::function<void()>& shader_setup,
		int entity_id
	);

	void DrawTexture(
		ShaderId shader, TextureId texture, const std::array<V2_float, 4>& positions, float depth,
		Color tint, const std::array<V2_float, 4>& tex_coords,
		const std::function<void()>& shader_setup, int entity_id
	);

	void DrawRenderPass(
		ShaderId shader, RenderPass& pass, RenderTargetId scene_render_target,
		const std::function<void()>& shader_setup
	);

	using BatchSetup = std::function<void(Renderer&)>;

	template <VertexType TVertex>
	struct DefaultTextureIndexAccessor {
		constexpr float& operator()(TVertex& vertex) const noexcept {
			return vertex.tex_index[0];
		}
	};

	template <VertexType TVertex, typename TAccessor = DefaultTextureIndexAccessor<TVertex>>
	void DrawTexturedQuads(
		std::string_view pipeline_name, ShaderId shader, std::span<TVertex> vertices,
		std::span<const std::uint32_t> local_indices, std::span<const TextureId> textures = {},
		std::optional<std::size_t> batch_state_hash = std::nullopt,
		const BatchSetup& batch_setup = {}, TAccessor get_tex_index = {}
	) {
		SetShader(shader);
		SetPipeline(pipeline_name, batch_state_hash, batch_setup);
		SubmitTexturedQuads<TVertex>(vertices, local_indices, textures, get_tex_index);
	}

	V2_int GetRenderTargetSize(RenderTargetId render_target) const;
	TextureFormat GetRenderTargetTextureFormat(RenderTargetId render_target) const;
	void ResizeRenderTarget(RenderTargetId render_target, V2_int new_size);
	void ClearRenderTarget(RenderTargetId render_target, Color color, bool set_viewport) const;
	void BindRenderTarget(RenderTargetId render_target);
	void BindRenderPass(RenderPass& render_pass);
	void BindScreenTarget();

	RenderTargetId GetScreenTarget() const;

	RenderPass BeginPass(RenderTargetId scene_render_target);

	V2_int GetTextureSize(TextureId id) const;
	TextureFormat GetTextureFormat(TextureId id) const;

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

	using PipelineId = std::size_t;

	using EventSink =
		std::function<void(V2_int, std::variant<ResizeType, impl::PresentationResizeType>)>;

	struct Pipeline {
		VertexArrayObject vao;
		VertexBufferObject vbo;
		ElementBufferObject ebo;
		std::uint32_t vertex_size{ 0 };
		std::optional<std::size_t> batch_state_hash_;
		BatchSetup batch_setup_;
		PrimitiveMode primitive_mode{ PrimitiveMode::Triangles };
	};

	Renderer() = delete;
	explicit Renderer(Window& window, EventSink&& event_sink);
	~Renderer() noexcept;
	Renderer(const Renderer&)				 = delete;
	Renderer(Renderer&&) noexcept			 = delete;
	Renderer& operator=(const Renderer&)	 = delete;
	Renderer& operator=(Renderer&&) noexcept = delete;

	void SetPipeline(
		std::string_view name, std::optional<std::size_t> batch_state_hash = std::nullopt,
		const BatchSetup& batch_setup = nullptr
	);

	void BeginFrame();
	void EndFrame();

	[[nodiscard]] bool IsPresentationViewportVisible() const;

	template <typename State, InvocableR<void> F>
	friend void UpdateStateIfChanged(Renderer&, const std::optional<State>&, const State&, F&&);

	struct TextureSlotInfo {
		std::uint32_t slot{ 0 };
		bool push_to_batch{ false };
	};

	std::size_t GetMaxTextureSlots() const;

	/// @return The texture slot the given texture is bound to, and whether it should be pushed to
	/// batch_textures.
	[[nodiscard]] TextureSlotInfo GetTextureSlot(TextureId tex);

	/// @brief True if adding the given number of vertex bytes and indices would exceed
	/// batch capacity.
	[[nodiscard]] bool ExceedsCapacity(
		std::size_t vertex_bytes, std::size_t indices, std::size_t vertex_byte_capacity
	) const;

	/// @return True if the given texture is currently attached to the framebuffer that is currently
	/// bound.
	bool IsTextureAttachedToCurrentFramebuffer(TextureId texture) const;

	void ResizeScreenTarget(V2_int size);

	void OnWindowResize(V2_int size);

	RenderTargetId AcquirePooledTargetCopy(RenderTargetId render_target);
	RenderTargetId AcquirePooledTarget(V2_int size, TextureFormat format);

	void ReleasePooledTarget(RenderTargetId render_target);

	void InvalidateState();

	Pipeline& GetCurrentPipeline();

	void SetCurrentPipelineBatchState(
		std::optional<std::size_t> batch_state_hash, const BatchSetup& batch_setup
	);

	// emit_events = false is used to prevent emitting events when initializing the window and
	// scene.
	void UpdateDisplayViewport(bool emit_events = true);

	[[nodiscard]] DisplayResizeInfo RecalculateDisplayViewport() const;

	template <VertexType TVertex, typename TAccessor = DefaultTextureIndexAccessor<TVertex>>
	void SubmitTexturedQuads(
		std::span<TVertex> vertices, std::span<const std::uint32_t> local_indices,
		std::span<const TextureId> local_textures = {}, TAccessor get_tex_index = {}
	) {
		static_assert(std::is_trivially_copyable_v<TVertex>);
		static_assert(std::is_standard_layout_v<TVertex>);

		constexpr std::size_t kVerticesPerQuad{ 4 };
		constexpr std::size_t kIndicesPerQuad{ 6 };

		PTGN_ASSERT(
			vertices.size() % kVerticesPerQuad == 0,
			"Textured quad submission expects 4 vertices per quad"
		);
		PTGN_ASSERT(
			local_indices.size() % kIndicesPerQuad == 0,
			"Textured quad submission expects 6 indices per quad"
		);

		std::vector<TVertex> chunk_vertices;
		std::vector<std::uint32_t> chunk_indices;

		chunk_vertices.reserve(std::min<std::size_t>(vertices.size(), kVertexCapacity));
		chunk_indices.reserve(std::min<std::size_t>(local_indices.size(), kIndexCapacity));

		auto flush_chunk = [&]() {
			if (chunk_vertices.empty()) {
				return;
			}

			SubmitVertices<TVertex>(chunk_vertices, chunk_indices);
			chunk_vertices.clear();
			chunk_indices.clear();
		};

		std::size_t quad_count{ vertices.size() / kVerticesPerQuad };

		for (std::size_t quad{ 0 }; quad < quad_count; ++quad) {
			std::size_t vertex_begin{ quad * kVerticesPerQuad };
			std::size_t index_begin{ quad * kIndicesPerQuad };

			TextureId texture{ 0 };
			float batch_texture_slot{ 0.0f };

			if (!local_textures.empty()) {
				auto local_texture_index{
					static_cast<std::size_t>(get_tex_index(vertices[vertex_begin]))
				};

				PTGN_ASSERT(
					local_texture_index < local_textures.size(),
					"Invalid local texture index in submitted vertex"
				);

				texture = local_textures[local_texture_index];

				if (bool texture_already_bound{ std::ranges::contains(batch_textures_, texture) };
					!texture_already_bound && batch_textures_.size() >= GetMaxTextureSlots()) {
					flush_chunk();
					FlushBatch();
				}

				auto slot_info{ GetTextureSlot(texture) };

				if (slot_info.push_to_batch) {
					batch_textures_.push_back(texture);
				}

				batch_texture_slot = static_cast<float>(slot_info.slot);
			}

			if (ExceedsCapacity(
					(chunk_vertices.size() + kVerticesPerQuad) * sizeof(TVertex),
					chunk_indices.size() + kIndicesPerQuad, kVertexCapacity * sizeof(TVertex)
				)) {
				flush_chunk();
			}

			PTGN_ASSERT(
				!ExceedsCapacity(
					kVerticesPerQuad * sizeof(TVertex), kIndicesPerQuad,
					kVertexCapacity * sizeof(TVertex)
				),
				"Single quad exceeds renderer batch capacity"
			);

			auto base_vertex{ static_cast<std::uint32_t>(chunk_vertices.size()) };

			for (std::size_t i{ 0 }; i < kVerticesPerQuad; ++i) {
				TVertex vertex{ vertices[vertex_begin + i] };

				if (!local_textures.empty()) {
					get_tex_index(vertex) = batch_texture_slot;
				}

				chunk_vertices.push_back(vertex);
			}

			for (std::size_t i{ 0 }; i < kIndicesPerQuad; ++i) {
				chunk_indices.push_back(base_vertex + local_indices[index_begin + i]);
			}
		}

		flush_chunk();
	}

	template <VertexType TVertex>
	void SubmitVertices(
		std::span<const TVertex> vertices, std::span<const std::uint32_t> local_indices
	) {
		static_assert(std::is_trivially_copyable_v<TVertex>);
		static_assert(std::is_standard_layout_v<TVertex>);

		auto vertex_bytes{ vertices.size() * sizeof(TVertex) };
		auto vertex_byte_capacity{ kVertexCapacity * sizeof(TVertex) };

		if (ExceedsCapacity(vertex_bytes, local_indices.size(), vertex_byte_capacity)) {
			FlushBatch();
		}

		PTGN_ASSERT(
			!ExceedsCapacity(vertex_bytes, local_indices.size(), vertex_byte_capacity),
			"Attempting to batch too many vertices or indices in one call"
		);

		auto base_vertex{ static_cast<std::uint32_t>(batch_vertices_.size() / sizeof(TVertex)) };

		auto bytes{ std::as_bytes(vertices) };

		batch_vertices_.insert(batch_vertices_.end(), bytes.begin(), bytes.end());

		batch_indices_.reserve(batch_indices_.size() + local_indices.size());
		for (auto idx : local_indices) {
			batch_indices_.push_back(base_vertex + idx);
		}
	}

	[[nodiscard]] ElementBufferObject CreateElementBufferObject(std::uint32_t index_capacity);
	[[nodiscard]] VertexBufferObject CreateVertexBufferObject(
		std::uint32_t vertex_capacity, std::uint32_t vertex_size
	);
	[[nodiscard]] VertexArrayObject CreateVertexArrayObject(
		VertexBufferId vertex_buffer, const BufferLayoutView& layout, ElementBufferId element_buffer
	);

	template <VertexType T>
	void AddPipeline(
		std::string_view name, std::uint32_t vertex_capacity, std::uint32_t index_capacity,
		PrimitiveMode primitive_mode
	) {
		Pipeline pipeline;
		pipeline.primitive_mode = primitive_mode;
		pipeline.vertex_size	= sizeof(typename T::VertexType);
		pipeline.ebo			= CreateElementBufferObject(index_capacity);
		pipeline.vbo			= CreateVertexBufferObject(vertex_capacity, pipeline.vertex_size);
		pipeline.vao = CreateVertexArrayObject(pipeline.vbo, T::GetLayoutView(), pipeline.ebo);

		pipelines_.emplace_back(Hash(name), std::move(pipeline));
	}

	Window& window_;

	EventSink event_sink_;

	std::unique_ptr<gl::GLContext> gl_;

	PipelineId current_pipeline_{ 0 };
	std::vector<std::pair<PipelineId, Pipeline>> pipelines_;

	std::vector<std::byte> batch_vertices_;
	std::vector<Index> batch_indices_;
	std::vector<TextureId> batch_textures_;

	/// @brief Currently set view projection.
	Matrix4 view_projection_;

	Color background_color_;
	RenderTargetObject screen_target_;

	std::optional<V2_int> game_size_;
	Viewport display_viewport_;
	ScalingMode scaling_mode_{ ScalingMode::Letterbox };

	std::vector<PooledTarget> rt_pool_;
	std::uint64_t pool_tick_{ 0 };
	std::size_t max_pool_size_{ 16 };

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