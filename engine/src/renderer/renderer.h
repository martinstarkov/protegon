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
#include "renderer/pipeline/render_batch.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
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

struct RenderTargetDesc {
	V2_int size{};
	TextureFormat format{ TextureFormat::RGBA8 };

	bool operator==(const RenderTargetDesc&) const = default;
};

enum class TextureRefKind {
	Texture,
	RenderTarget,
	BoundTarget,
};

struct TextureRef {
	TextureRefKind kind{ TextureRefKind::Texture };
	TextureId texture{};
	RenderTargetObject* target{ nullptr };

	static TextureRef Texture(TextureId texture) {
		return {
			.kind	 = TextureRefKind::Texture,
			.texture = texture,
			.target	 = nullptr,
		};
	}

	static TextureRef Target(RenderTargetObject& target) {
		return {
			.kind	 = TextureRefKind::RenderTarget,
			.texture = {},
			.target	 = &target,
		};
	}

	static TextureRef BoundTarget() {
		return {
			.kind	 = TextureRefKind::BoundTarget,
			.texture = {},
			.target	 = nullptr,
		};
	}

	bool IsBoundTarget() const {
		return kind == TextureRefKind::BoundTarget;
	}
};

struct TextureBinding {
	std::string name;
	TextureRef texture;
};

struct DrawTextureOptions {
	RenderState render_state{};
	std::vector<TextureBinding> extra_textures;
};

struct PassDrawOptions {
	RenderState render_state{};
	std::vector<TextureBinding> extra_textures;
};

struct RenderStream {
	RenderTargetObject* latest{ nullptr };
	bool latest_from_pool{ false };
	RenderTargetDesc desc{};
};

struct EffectParams {
	std::function<void(DrawContext&)> draw_callback;

	int margin{ 0 };
};

class Renderer {
public:
	void BeginScene(RenderTargetObject& scene_target, Color clear_color) {
		FlushBatch();

		stream_stack_.clear();

		scene_stream_ = RenderStream{
			.latest = &scene_target,
			.latest_from_pool = false,
			.desc = {
				.size = scene_target.GetSize(),
				.format = scene_target.GetFormat(),
			},
		};

		scene_target.Bind();
		SetViewport({ .position{}, .size = scene_target.GetSize() });
		scene_target.Clear(clear_color, false);
	}

	RenderTargetObject& EndScene() {
		FlushBatch();

		PTGN_ASSERT(scene_stream_.latest);
		return *scene_stream_.latest;
	}

	TextureRef BoundTarget() const {
		return TextureRef::BoundTarget();
	}

	template <VertexType TVertex>
	struct DefaultTextureIndexAccessor {
		float Get(const TVertex& vertex) const noexcept {
			return vertex.tex_index[0];
		}

		void Set(TVertex& vertex, float value) const noexcept {
			vertex.tex_index[0] = value;
		}
	};

	template <VertexType TVertex, typename TAccessor = DefaultTextureIndexAccessor<TVertex>>
	void DrawTexturedGeometry(
		std::string_view pipeline_name, const MaterialState& material,
		std::span<const TVertex> vertices, std::span<const std::uint32_t> local_indices,
		std::span<const TextureId> local_textures = {}, RenderState render_state = {},
		TAccessor texture_index = {}
	) {
		PTGN_ASSERT(CurrentStream().latest, "No active render stream");
		PTGN_ASSERT(material.shader != 0, "Material shader cannot be null");

		const PipelineId pipeline_id = Hash(pipeline_name);

		BatchKey key{
			.pipeline_id  = pipeline_id,
			.target		  = CurrentStream().latest,
			.render_state = render_state,
			.material	  = material,
		};

		UseBatchKey(key);

		SubmitTexturedGeometry<TVertex>(
			key, vertices, local_indices, local_textures, texture_index
		);
	}

	// ------------------------------------------------------------
	// DrawTexture convenience API.
	// ------------------------------------------------------------

	void DrawTexture(
		const MaterialState& material, TextureRef texture, const std::array<V2_float, 4>& positions,
		float depth, Color tint, const std::array<V2_float, 4>& tex_coords,
		const DrawTextureOptions& options = {}, int entity_id = -1,
		const std::optional<EffectParams>& effect_params = std::nullopt
	) {
		PTGN_ASSERT(material.shader != 0, "Material shader cannot be null");

		if (effect_params.has_value()) {
			DrawTextureWithEffects(
				material, texture, positions, depth, tint, tex_coords, options, entity_id,
				*effect_params
			);
			return;
		}

		if (texture.kind == TextureRefKind::BoundTarget) {
			DrawBoundTargetEffect(material, positions, depth, tint, tex_coords, options);
			return;
		}

		if (!options.extra_textures.empty()) {
			DrawImmediateTexturedQuad(
				material, texture, positions, depth, tint, tex_coords, options.render_state,
				options.extra_textures
			);
			return;
		}

		DrawBatchedTexture(
			material, texture, positions, depth, tint, tex_coords, options.render_state, entity_id
		);
	}

	TextureRef DrawPassToTransient(
		const MaterialState& material, TextureRef input, RenderTargetDesc output_desc,
		const PassDrawOptions& options = {}
	) {
		FlushBatch();

		RenderStream& stream = CurrentStream();

		RenderTargetObject* input_target = ResolveTargetOrNull(input, stream);
		RenderTargetObject& output		 = AcquirePooledTarget(output_desc, input_target);

		output.Bind();
		SetViewport({ .position{}, .size = output_desc.size });
		output.Clear(color::Transparent, false);

		auto quad = FullscreenQuad(output_desc.size);
		auto uvs  = DefaultTextureCoordinates();

		DrawImmediateTexturedQuad(
			material, input, quad, 0.0f, color::White, uvs, options.render_state,
			options.extra_textures
		);

		return TextureRef::Target(output);
	}

	void Release(TextureRef ref) {
		if (ref.kind != TextureRefKind::RenderTarget || !ref.target) {
			return;
		}

		ReleasePooledTarget(*ref.target);
	}

	// ------------------------------------------------------------
	// Pooled render targets.
	// ------------------------------------------------------------

	RenderTargetObject& AcquirePooledTarget(
		RenderTargetDesc desc, const RenderTargetObject* exclude = nullptr
	) {
		++pool_tick_;

		PooledRenderTarget* exact				 = nullptr;
		PooledRenderTarget* reusable_same_format = nullptr;

		for (auto& ptr : rt_pool_) {
			PooledRenderTarget& entry = *ptr;

			if (entry.in_use) {
				continue;
			}

			if (&entry.target == exclude) {
				continue;
			}

			if (entry.target.GetFormat() != desc.format) {
				continue;
			}

			if (entry.target.GetSize() == desc.size) {
				exact = &entry;
				break;
			}

			if (!reusable_same_format ||
				entry.last_used_tick < reusable_same_format->last_used_tick) {
				reusable_same_format = &entry;
			}
		}

		auto claim = [&](PooledRenderTarget& entry) -> RenderTargetObject& {
			if (entry.target.GetSize() != desc.size) {
				entry.target.Resize(desc.size);
			}

			entry.in_use		 = true;
			entry.last_used_tick = pool_tick_;

			entry.target.Bind();
			entry.target.Clear(color::Transparent, false);

			return entry.target;
		};

		if (exact) {
			return claim(*exact);
		}

		if (reusable_same_format) {
			return claim(*reusable_same_format);
		}

		auto entry			  = std::make_unique<PooledRenderTarget>();
		entry->target		  = CreateRenderTarget(desc.size, desc.format);
		entry->in_use		  = true;
		entry->last_used_tick = pool_tick_;

		RenderTargetObject& target = entry->target;
		rt_pool_.push_back(std::move(entry));

		target.Bind();
		target.Clear(color::Transparent, false);

		return target;
	}

	RenderTargetObject& AcquirePooledTargetLike(
		const RenderTargetObject& source, int margin = 0,
		const RenderTargetObject* exclude = nullptr
	) {
		RenderTargetDesc desc{
			.size	= source.GetSize(),
			.format = source.GetFormat(),
		};

		desc.size.x += margin * 2;
		desc.size.y += margin * 2;

		return AcquirePooledTarget(desc, exclude);
	}

	void ReleasePooledTarget(RenderTargetObject& target) {
		for (auto& ptr : rt_pool_) {
			PooledRenderTarget& entry = *ptr;

			if (&entry.target != &target) {
				continue;
			}

			PTGN_ASSERT(entry.in_use, "Attempting to release a pooled target that is not in use");

			entry.in_use		 = false;
			entry.last_used_tick = ++pool_tick_;
			return;
		}

		PTGN_ERROR("Attempting to release a render target that is not owned by the pool");
	}

private:
	// ------------------------------------------------------------
	// Batch state.
	// ------------------------------------------------------------

	struct BatchKey {
		PipelineId pipeline_id{};
		RenderTargetObject* target{};
		RenderState render_state{};
		MaterialState material{};

		bool operator==(const BatchKey&) const = default;
	};

	struct TextureSlotInfo {
		std::uint32_t slot{ 0 };
		bool push_to_batch{ false };
	};

	void UseBatchKey(const BatchKey& key) {
		if (batch_key_.has_value() && *batch_key_ == key) {
			return;
		}

		FlushBatch();

		PTGN_ASSERT(pipelines_.HasPipeline(key.pipeline_id), "No matching render pipeline");
		pipelines_.SetCurrentPipeline(key.pipeline_id);

		batch_key_ = key;
	}

	void FlushBatchAndContinue(const BatchKey& key) {
		FlushBatch();

		PTGN_ASSERT(pipelines_.HasPipeline(key.pipeline_id), "No matching render pipeline");
		pipelines_.SetCurrentPipeline(key.pipeline_id);

		batch_key_ = key;
	}

	void FlushBatch() {
		if (batch_indices_.empty()) {
			ReleasePendingTargetsAfterFlush();
			batch_key_.reset();
			return;
		}

		PTGN_ASSERT(batch_key_.has_value(), "Batch has data without a batch key");

		const BatchKey key		 = *batch_key_;
		RenderPipeline& pipeline = pipelines_.GetPipeline(key.pipeline_id);

		key.target->Bind();
		ApplyRenderState(key.render_state);
		ApplyMaterial(key.material);

		BindPipelineBuffers(pipeline);

		const auto vertex_count =
			static_cast<std::uint32_t>(batch_vertices_.size() / pipeline.vertex_size);

		UploadVertexData(pipeline, batch_vertices_.data(), vertex_count);
		UploadIndexData(
			pipeline, batch_indices_.data(), static_cast<std::uint32_t>(batch_indices_.size())
		);

		for (std::uint32_t slot = 0; slot < batch_textures_.size(); ++slot) {
			BindTextureSlot(slot, batch_textures_[slot]);
		}

		DrawIndexed(pipeline, static_cast<std::uint32_t>(batch_indices_.size()));

		batch_vertices_.clear();
		batch_indices_.clear();
		batch_textures_.clear();
		batch_key_.reset();

		ReleasePendingTargetsAfterFlush();
	}

	void ReleasePendingTargetsAfterFlush() {
		for (RenderTargetObject* target : release_after_flush_) {
			ReleasePooledTarget(*target);
		}

		release_after_flush_.clear();
	}

	void HoldPooledTargetUntilBatchFlush(RenderTargetObject& target) {
		if (!IsPooledTarget(target)) {
			return;
		}

		if (!std::ranges::contains(release_after_flush_, &target)) {
			release_after_flush_.push_back(&target);
		}
	}

	bool IsPooledTarget(const RenderTargetObject& target) const {
		return std::ranges::any_of(rt_pool_, [&target](const auto& ptr) {
			return &ptr->target == &target;
		});
	}

	TextureSlotInfo GetTextureSlotNoFlush(TextureId texture) {
		for (std::uint32_t i = 0; i < batch_textures_.size(); ++i) {
			if (batch_textures_[i] == texture) {
				return {
					.slot		   = i,
					.push_to_batch = false,
				};
			}
		}

		return {
			.slot		   = static_cast<std::uint32_t>(batch_textures_.size()),
			.push_to_batch = true,
		};
	}

	template <VertexType TVertex, typename TAccessor = DefaultTextureIndexAccessor<TVertex>>
	void SubmitTexturedGeometry(
		const BatchKey& key, std::span<const TVertex> vertices,
		std::span<const std::uint32_t> local_indices, std::span<const TextureId> local_textures,
		TAccessor texture_index
	) {
		static_assert(std::is_trivially_copyable_v<TVertex>);
		static_assert(std::is_standard_layout_v<TVertex>);

		constexpr std::size_t kVerticesPerQuad{ 4 };
		constexpr std::size_t kIndicesPerQuad{ 6 };

		PTGN_ASSERT(
			vertices.size() % kVerticesPerQuad == 0, "Textured geometry expects 4 vertices per quad"
		);

		PTGN_ASSERT(
			local_indices.size() % kIndicesPerQuad == 0,
			"Textured geometry expects 6 indices per quad"
		);

		std::vector<TVertex> chunk_vertices;
		std::vector<std::uint32_t> chunk_indices;

		chunk_vertices.reserve(std::min<std::size_t>(vertices.size(), kVertexCapacity));
		chunk_indices.reserve(std::min<std::size_t>(local_indices.size(), kIndexCapacity));

		auto flush_chunk = [&]() {
			if (chunk_vertices.empty()) {
				return;
			}

			SubmitVertices<TVertex>(key, chunk_vertices, chunk_indices);

			chunk_vertices.clear();
			chunk_indices.clear();
		};

		const std::size_t quad_count = vertices.size() / kVerticesPerQuad;

		for (std::size_t quad = 0; quad < quad_count; ++quad) {
			const std::size_t vertex_begin = quad * kVerticesPerQuad;
			const std::size_t index_begin  = quad * kIndicesPerQuad;

			float batch_texture_slot = 0.0f;

			if (!local_textures.empty()) {
				const auto local_texture_index =
					static_cast<std::size_t>(texture_index.Get(vertices[vertex_begin]));

				PTGN_ASSERT(
					local_texture_index < local_textures.size(), "Invalid local texture index"
				);

				const TextureId texture = local_textures[local_texture_index];

				PTGN_ASSERT(texture != 0, "Texture cannot be null");

				if (IsTextureAttachedToCurrentFramebuffer(texture)) {
					PTGN_ERROR("Cannot sample a texture attached to the current framebuffer");
				}

				const bool already_bound = std::ranges::contains(batch_textures_, texture);

				if (!already_bound && batch_textures_.size() >= GetMaxTextureSlots()) {
					flush_chunk();
					FlushBatchAndContinue(key);
				}

				const TextureSlotInfo slot = GetTextureSlotNoFlush(texture);

				if (slot.push_to_batch) {
					batch_textures_.push_back(texture);
				}

				batch_texture_slot = static_cast<float>(slot.slot);
			}

			if (ChunkExceedsCapacity<TVertex>(
					chunk_vertices.size() + kVerticesPerQuad,
					chunk_indices.size() + kIndicesPerQuad, key
				)) {
				flush_chunk();
			}

			const auto base_vertex = static_cast<std::uint32_t>(chunk_vertices.size());

			for (std::size_t i = 0; i < kVerticesPerQuad; ++i) {
				TVertex vertex = vertices[vertex_begin + i];

				if (!local_textures.empty()) {
					texture_index.Set(vertex, batch_texture_slot);
				}

				chunk_vertices.push_back(vertex);
			}

			for (std::size_t i = 0; i < kIndicesPerQuad; ++i) {
				chunk_indices.push_back(base_vertex + local_indices[index_begin + i]);
			}
		}

		flush_chunk();
	}

	template <VertexType TVertex>
	void SubmitVertices(
		const BatchKey& key, std::span<const TVertex> vertices,
		std::span<const std::uint32_t> local_indices
	) {
		static_assert(std::is_trivially_copyable_v<TVertex>);
		static_assert(std::is_standard_layout_v<TVertex>);

		const RenderPipeline& pipeline = pipelines_.GetPipeline(key.pipeline_id);

		const std::size_t vertex_bytes = vertices.size() * sizeof(TVertex);
		const std::size_t vertex_capacity_bytes =
			static_cast<std::size_t>(pipeline.vertex_capacity) * pipeline.vertex_size;

		PTGN_ASSERT(
			vertex_bytes <= vertex_capacity_bytes,
			"Single geometry submit exceeds vertex buffer capacity"
		);

		PTGN_ASSERT(
			local_indices.size() <= pipeline.index_capacity,
			"Single geometry submit exceeds index buffer capacity"
		);

		if (batch_vertices_.size() + vertex_bytes > vertex_capacity_bytes ||
			batch_indices_.size() + local_indices.size() > pipeline.index_capacity) {
			FlushBatchAndContinue(key);
		}

		const auto base_vertex =
			static_cast<std::uint32_t>(batch_vertices_.size() / sizeof(TVertex));

		const auto bytes = std::as_bytes(vertices);

		batch_vertices_.insert(batch_vertices_.end(), bytes.begin(), bytes.end());

		batch_indices_.reserve(batch_indices_.size() + local_indices.size());

		for (std::uint32_t index : local_indices) {
			batch_indices_.push_back(base_vertex + index);
		}
	}

	template <VertexType TVertex>
	bool ChunkExceedsCapacity(
		std::size_t chunk_vertices, std::size_t chunk_indices, const BatchKey& key
	) const {
		const RenderPipeline& pipeline = pipelines_.GetPipeline(key.pipeline_id);

		return chunk_vertices > pipeline.vertex_capacity || chunk_indices > pipeline.index_capacity;
	}

	// ------------------------------------------------------------
	// DrawTexture internals.
	// ------------------------------------------------------------

	void DrawBatchedTexture(
		const MaterialState& material, TextureRef texture, const std::array<V2_float, 4>& positions,
		float depth, Color tint, const std::array<V2_float, 4>& tex_coords,
		RenderState render_state, int entity_id
	) {
		TextureId texture_id = ResolveTexture(texture, CurrentStream());

		const auto color_n = tint.Normalized();

		std::array<TextureVertex, 4> vertices{
			TextureVertex{ positions[0], depth, color_n, tex_coords[0], 0.0f, entity_id },
			TextureVertex{ positions[1], depth, color_n, tex_coords[1], 0.0f, entity_id },
			TextureVertex{ positions[2], depth, color_n, tex_coords[2], 0.0f, entity_id },
			TextureVertex{ positions[3], depth, color_n, tex_coords[3], 0.0f, entity_id },
		};

		constexpr std::array<std::uint32_t, 6> indices{ 0, 1, 2, 2, 3, 0 };
		const std::array<TextureId, 1> textures{ texture_id };

		DrawTexturedGeometry<TextureVertex>(
			"texture", material, vertices, indices, textures, render_state
		);
	}

	void DrawBoundTargetEffect(
		const MaterialState& material, const std::array<V2_float, 4>& positions, float depth,
		Color tint, const std::array<V2_float, 4>& tex_coords, const DrawTextureOptions& options
	) {
		FlushBatch();

		RenderStream& stream = CurrentStream();

		PTGN_ASSERT(stream.latest, "No bound target stream");

		RenderTargetObject& input  = *stream.latest;
		RenderTargetObject& output = AcquirePooledTarget(stream.desc, &input);

		output.Bind();
		SetViewport({ .position{}, .size = stream.desc.size });
		output.Clear(color::Transparent, false);

		DrawImmediateTexturedQuad(
			material, TextureRef::Target(input), positions, depth, tint, tex_coords,
			options.render_state, options.extra_textures
		);

		if (stream.latest_from_pool) {
			ReleasePooledTarget(*stream.latest);
		}

		stream.latest			= &output;
		stream.latest_from_pool = true;
	}

	void DrawTextureWithEffects(
		const MaterialState& source_material, TextureRef source_texture,
		const std::array<V2_float, 4>& world_positions, float depth, Color tint,
		const std::array<V2_float, 4>& tex_coords, const DrawTextureOptions& options, int entity_id,
		const EffectParams& effect_params
	) {
		FlushBatch();

		const int margin = std::max(0, effect_params.margin);

		RenderTargetDesc source_desc  = GetTextureRefDesc(source_texture, CurrentStream());
		RenderTargetDesc local_desc	  = source_desc;
		local_desc.size.x			 += margin * 2;
		local_desc.size.y			 += margin * 2;

		RenderTargetObject& local_target = AcquirePooledTarget(local_desc);

		local_target.Bind();
		SetViewport({ .position{}, .size = local_desc.size });
		local_target.Clear(color::Transparent, false);

		auto local_positions  = QuadInsidePaddedTarget(source_desc.size, margin);
		auto local_tex_coords = tex_coords;

		DrawImmediateTexturedQuad(
			source_material, source_texture, local_positions, 0.0f, tint, local_tex_coords,
			options.render_state, {}
		);

		RenderStream local_stream{
			.latest			  = &local_target,
			.latest_from_pool = true,
			.desc			  = local_desc,
		};

		PushStream(local_stream);

		DrawContext effect_context{ *this };

		effect_params.draw_callback(effect_context);

		PopStream();

		RenderTargetObject& final_target = *local_stream.latest;

		MaterialState copy_material{
			.shader	  = GetShader("texture"),
			.uniforms = {},
		};

		auto expanded_positions = ExpandQuadByPixels(world_positions, source_desc.size, margin);

		DrawBatchedTexture(
			copy_material, TextureRef::Target(final_target), expanded_positions, depth,
			color::White, DefaultTextureCoordinates(), options.render_state, entity_id
		);

		HoldPooledTargetUntilBatchFlush(final_target);

		// If the final target is not the original local target, then the stream effect chain
		// already released the old targets as it advanced. If no effect wrote anything, this
		// final target is local_target and is held until the batch sampling it is flushed.
	}

	void DrawImmediateTexturedQuad(
		const MaterialState& material, TextureRef primary_texture,
		const std::array<V2_float, 4>& positions, float depth, Color tint,
		const std::array<V2_float, 4>& tex_coords, RenderState render_state,
		std::span<const TextureBinding> extra_textures
	) {
		FlushBatch();

		PTGN_ASSERT(CurrentStream().latest, "No active stream");

		CurrentStream().latest->Bind();
		ApplyRenderState(render_state);
		ApplyMaterial(material);

		TextureId primary = ResolveTexture(primary_texture, CurrentStream());

		BindTextureSlot(0, primary);
		SetUniform(material.shader, "u_Texture", 0);

		int slot = 1;

		for (const TextureBinding& binding : extra_textures) {
			TextureId texture = ResolveTexture(binding.texture, CurrentStream());

			BindTextureSlot(static_cast<std::uint32_t>(slot), texture);
			SetUniform(material.shader, binding.name.c_str(), slot);

			++slot;
		}

		const auto color_n = tint.Normalized();

		std::array<TextureVertex, 4> vertices{
			TextureVertex{ positions[0], depth, color_n, tex_coords[0], 0.0f, -1 },
			TextureVertex{ positions[1], depth, color_n, tex_coords[1], 0.0f, -1 },
			TextureVertex{ positions[2], depth, color_n, tex_coords[2], 0.0f, -1 },
			TextureVertex{ positions[3], depth, color_n, tex_coords[3], 0.0f, -1 },
		};

		constexpr std::array<std::uint32_t, 6> indices{ 0, 1, 2, 2, 3, 0 };

		DrawImmediateVertices("texture", material, vertices, indices);
	}

	template <VertexType TVertex>
	void DrawImmediateVertices(
		std::string_view pipeline_name, const MaterialState& material,
		std::span<const TVertex> vertices, std::span<const std::uint32_t> indices
	) {
		const PipelineId pipeline_id = Hash(pipeline_name);
		RenderPipeline& pipeline	 = pipelines_.GetPipeline(pipeline_id);

		pipelines_.SetCurrentPipeline(pipeline_id);

		BindPipelineBuffers(pipeline);

		UploadVertexData(
			pipeline, std::as_bytes(vertices).data(), static_cast<std::uint32_t>(vertices.size())
		);

		UploadIndexData(pipeline, indices.data(), static_cast<std::uint32_t>(indices.size()));

		DrawIndexed(pipeline, static_cast<std::uint32_t>(indices.size()));
	}

	// ------------------------------------------------------------
	// Stream helpers.
	// ------------------------------------------------------------

	RenderStream& CurrentStream() {
		if (!stream_stack_.empty()) {
			return *stream_stack_.back();
		}

		return scene_stream_;
	}

	const RenderStream& CurrentStream() const {
		if (!stream_stack_.empty()) {
			return *stream_stack_.back();
		}

		return scene_stream_;
	}

	void PushStream(RenderStream& stream) {
		stream_stack_.push_back(&stream);
	}

	void PopStream() {
		PTGN_ASSERT(!stream_stack_.empty(), "Cannot pop an empty render stream stack");
		stream_stack_.pop_back();
	}

	// ------------------------------------------------------------
	// Texture resolving.
	// ------------------------------------------------------------

	TextureId ResolveTexture(TextureRef ref, const RenderStream& stream) const {
		switch (ref.kind) {
			case TextureRefKind::Texture: return ref.texture;

			case TextureRefKind::RenderTarget:
				PTGN_ASSERT(ref.target);
				return ref.target->GetTextureId();

			case TextureRefKind::BoundTarget:
				PTGN_ASSERT(stream.latest);
				return stream.latest->GetTextureId();
		}

		PTGN_ERROR("Unknown TextureRef kind");
	}

	RenderTargetObject* ResolveTargetOrNull(TextureRef ref, const RenderStream& stream) const {
		switch (ref.kind) {
			case TextureRefKind::Texture:	   return nullptr;

			case TextureRefKind::RenderTarget: return ref.target;

			case TextureRefKind::BoundTarget:  return stream.latest;
		}

		return nullptr;
	}

	RenderTargetDesc GetTextureRefDesc(TextureRef ref, const RenderStream& stream) const {
		switch (ref.kind) {
			case TextureRefKind::Texture:
				return {
					.size	= GetTextureSize(ref.texture),
					.format = GetTextureFormat(ref.texture),
				};

			case TextureRefKind::RenderTarget:
				PTGN_ASSERT(ref.target);
				return {
					.size	= ref.target->GetSize(),
					.format = ref.target->GetFormat(),
				};

			case TextureRefKind::BoundTarget:
				PTGN_ASSERT(stream.latest);
				return {
					.size	= stream.latest->GetSize(),
					.format = stream.latest->GetFormat(),
				};
		}

		PTGN_ERROR("Unknown TextureRef kind");
	}

	// ------------------------------------------------------------
	// Material / render state.
	// ------------------------------------------------------------

	void ApplyRenderState(const RenderState& render_state) {
		if (render_state.blend_mode.has_value()) {
			SetBlendMode(*render_state.blend_mode);
		}
	}

	void ApplyMaterial(const MaterialState& material) {
		BindShader(material.shader);

		for (const UniformWrite& write : material.uniforms) {
			ApplyUniform(material.shader, write);
		}
	}

	void ApplyUniform(ShaderId shader, const UniformWrite& write) {
		std::visit(
			[&]<typename T>(const T& value) { SetUniform(shader, write.name.c_str(), value); },
			write.value
		);
	}

	// ------------------------------------------------------------
	// Geometry helpers.
	// ------------------------------------------------------------

	static std::array<V2_float, 4> DefaultTextureCoordinates() {
		return {
			V2_float{ 0.0f, 0.0f },
			V2_float{ 1.0f, 0.0f },
			V2_float{ 1.0f, 1.0f },
			V2_float{ 0.0f, 1.0f },
		};
	}

	static std::array<V2_float, 4> FullscreenQuad(V2_int size) {
		V2_float half{ static_cast<float>(size.x) * 0.5f, static_cast<float>(size.y) * 0.5f };

		return {
			V2_float{ -half.x, -half.y },
			V2_float{ half.x, -half.y },
			V2_float{ half.x, half.y },
			V2_float{ -half.x, half.y },
		};
	}

	static std::array<V2_float, 4> QuadInsidePaddedTarget(V2_int source_size, int margin) {
		const float w = static_cast<float>(source_size.x);
		const float h = static_cast<float>(source_size.y);

		const float left   = -w * 0.5f;
		const float right  = w * 0.5f;
		const float top	   = -h * 0.5f;
		const float bottom = h * 0.5f;

		// The target itself is larger by margin on all sides. Since this quad is centered,
		// the original texture is drawn into the center, leaving transparent border room.
		(void)margin;

		return {
			V2_float{ left, top },
			V2_float{ right, top },
			V2_float{ right, bottom },
			V2_float{ left, bottom },
		};
	}

	static std::array<V2_float, 4> ExpandQuadByPixels(
		std::array<V2_float, 4> quad, V2_int source_size, int margin
	) {
		if (margin <= 0) {
			return quad;
		}

		const float sx =
			static_cast<float>(source_size.x + margin * 2) / static_cast<float>(source_size.x);
		const float sy =
			static_cast<float>(source_size.y + margin * 2) / static_cast<float>(source_size.y);

		V2_float center{};
		for (const auto& p : quad) {
			center.x += p.x;
			center.y += p.y;
		}
		center.x *= 0.25f;
		center.y *= 0.25f;

		for (auto& p : quad) {
			p.x = center.x + (p.x - center.x) * sx;
			p.y = center.y + (p.y - center.y) * sy;
		}

		return quad;
	}

private:
	RenderStream scene_stream_{};
	std::vector<RenderStream*> stream_stack_;

	std::vector<std::unique_ptr<PooledRenderTarget>> rt_pool_;
	std::uint64_t pool_tick_{ 0 };
	std::size_t max_pool_size_{ 32 };

	std::optional<BatchKey> batch_key_;
	std::vector<std::byte> batch_vertices_;
	std::vector<Index> batch_indices_;
	std::vector<TextureId> batch_textures_;
	std::vector<RenderTargetObject*> release_after_flush_;

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
	[[nodiscard]] RenderTargetObject CreateRenderTarget(
		V2_int size, TextureFormat format, TextureParameters params = {}
	);

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

	// TODO: Move to private.
	void Execute(RenderGraph& graph);

	const DebugRenderGraphSnapshot& GetLastRenderGraphSnapshot() const {
		return last_graph_snapshot_;
	}

private:
	friend class ptgn::Application;
	friend class ApplicationContext;
	friend class RenderPipelineManager;

	void Compile(RenderGraph& graph);

	void DrawPacketImmediate(
		const RenderPacket& packet, const RenderState& state,
		std::span<const ResolvedTextureBinding> texture_bindings
	);

	void ExecuteFullscreenNode(const RenderGraph& graph, const RenderNode& node);

	void ExecuteClearNode(const RenderGraph& graph, const RenderNode& node);

	[[nodiscard]] IndexedPrimitiveGeometry<TextureVertex> MakeFullscreenTextureGeometry(
		V2_int size, bool flip_y
	) const;

	TextureId GetResourceTexture(const RenderGraph& graph, TextureNode texture) const;

	[[nodiscard]] TextureId ResolveTextureSource(
		const RenderGraph& graph, const TextureSource& source
	) const;

	[[nodiscard]] std::vector<ResolvedTextureBinding> ResolveTextureBindings(
		const RenderGraph& graph, std::span<const TextureBinding> bindings
	) const;

	std::uint32_t GetBatchTextureCount() const;

	[[nodiscard]] bool HasBatchTexture(TextureId texture) const;

	std::uint32_t GetCurrentVertexSize() const;

	std::uint32_t GetCurrentVertexCount() const;

	void AppendPrimitiveToBatch(const RenderPacket& packet, const PrimitiveRange& primitive);

	void EnsureBatchCanFit(
		PipelineId pipeline, const MaterialState& material, const RenderState& state,
		std::uint32_t vertex_size, bool has_texture_index_offset,
		const PrimitiveRequirements& requirements
	);

	[[nodiscard]] bool CanFitInCurrentBatch(const PrimitiveRequirements& requirements) const;

	void SubmitRenderPacket(const RenderPacket& packet, const RenderState& state);

	RenderTargetId GetResourceTarget(const RenderGraph& graph, TargetNode target) const;

	[[nodiscard]] std::uint32_t AddBatchTexture(TextureId texture);

	[[nodiscard]] RenderTargetId AcquirePooledTarget(V2_int size, TextureFormat format);

	void ReleasePooledTarget(RenderTargetId render_target);

	void TrimRenderTargetPool();

	// This figures out when each graph resource is first and last used.
	// The result tells Compile() how long each resource needs to keep its physical target.
	[[nodiscard]] std::vector<ResourceLifetime> ComputeResourceLifetimes(const RenderGraph& graph
	) const;

	// This runs after graph execution.
	// It releases all physical targets that were assigned to transient resources back into the
	// pool.
	void ReleaseCompiledTransients(RenderGraph& graph);

	void ExecuteNode(const RenderGraph& graph, const RenderNode& node);

	void ExecuteDrawLayerNode(const RenderGraph& graph, const RenderNode& node);

	void ApplyRenderStateToBackend(const RenderState& render_state);

	void SetUniformValue(ShaderId id, const char* uniform_name, const UniformValue& v);

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

	void BindTextureUnit(TextureId texture, std::uint32_t texture_unit);

	void ApplyTextureBindings(
		ShaderId shader, const RenderPipeline& pipeline,
		std::span<const ResolvedTextureBinding> bindings
	);

	void UploadVertices(const RenderPipeline& pipeline, std::span<const std::byte> vertices);
	void UploadIndices(const RenderPipeline& pipeline, std::span<const Index> indices);
	void DrawElements(const RenderPipeline& pipeline, std::uint32_t index_count);

	void ApplyMaterialUniforms(const MaterialState& material);

	void ApplyMaterial(const MaterialState& material);

	void BeginFrame();
	void EndFrame();

	void SetCurrentPipeline(std::size_t id);
	void SetCurrentPipeline(std::string_view name);

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

	void BuildDebugEdges(DebugRenderGraphSnapshot& snapshot, const RenderGraph& graph) const;
	[[nodiscard]] DebugRenderGraphSnapshot BuildDebugSnapshot(const RenderGraph& graph) const;

	Window& window_;

	EventSink event_sink_;

	std::unique_ptr<gl::GLContext> gl_;

	/// @brief Currently set view projection.
	Matrix4 view_projection_;

	Batch batch_;

	Color background_color_;
	RenderTargetObject screen_target_;

	std::optional<V2_int> game_size_;
	Viewport display_viewport_;
	ScalingMode scaling_mode_{ ScalingMode::Letterbox };

	// TODO: Move these to pool manager class.
	std::size_t max_pool_size_{ 32 };
	std::vector<PooledRenderTarget> rt_pool_;
	std::uint64_t pool_tick_{ 0 };

	/// @brief The viewport used for presentation (i.e. the final output to the screen). This
	/// may be different from the window if using the editor, which has a separate viewport for
	/// the game view.
	std::optional<Viewport> presentation_viewport_;

	/// @brief Flag to indicate whether the display viewport needs to be recalculated.
	bool display_viewport_dirty_{ true };

	std::optional<Camera> primary_world_camera_;

	RenderPipelineManager pipeline_manager_;
};

} // namespace impl

} // namespace ptgn