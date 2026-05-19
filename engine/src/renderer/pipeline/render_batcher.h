#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <tuple>
#include <type_traits>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/resources/id.h"
#include "renderer/vertex/vertex.h"

namespace ptgn::impl {

class Renderer;

using Index = std::uint32_t;

inline constexpr std::uint32_t kBatchCapacity{ 10000 };
inline constexpr std::uint32_t kVertexCapacity{ kBatchCapacity * 4 };
inline constexpr std::uint32_t kIndexCapacity{ kBatchCapacity * 6 };

template <VertexType TVertex>
using RenderQuad = std::array<TVertex, 4>;

inline RenderQuad<TextureVertex> CreateRenderQuad(
	const std::array<V2_float, 4>& positions, float depth = 0.0f,
	V4_float color_n						  = color::White.Normalized(),
	const std::array<V2_float, 4>& tex_coords = GetDefaultTextureCoordinates<false>(),
	float tex_index = 0.0f, int entity_id = -1
) {
	return {
		TextureVertex{ positions[0], depth, color_n, tex_coords[0], tex_index, entity_id },
		TextureVertex{ positions[1], depth, color_n, tex_coords[1], tex_index, entity_id },
		TextureVertex{ positions[2], depth, color_n, tex_coords[2], tex_index, entity_id },
		TextureVertex{ positions[3], depth, color_n, tex_coords[3], tex_index, entity_id },
	};
}

template <VertexType TVertex>
using RenderTriangle = std::array<TVertex, 3>;

inline constexpr std::array<Index, 6> kQuadIndices{
	0, 1, 2, 2, 3, 0,
};

inline constexpr std::array<Index, 3> kTriangleIndices{
	0,
	1,
	2,
};

template <VertexType TVertex>
struct DefaultTextureIndexAccessor {
	constexpr float& operator()(TVertex& vertex) const noexcept {
		return vertex.tex_index[0];
	}
};

struct NoTextureIndexAccessor {
	constexpr float& operator()(auto&) const noexcept {
		static float dummy{ 0.0f };
		return dummy;
	}
};

class RenderBatcher {
public:
	explicit RenderBatcher(Renderer& renderer);

	void Flush();

	void HoldUntilFlush(const RenderTargetObject& target);

	template <VertexType TVertex, typename TAccessor = DefaultTextureIndexAccessor<TVertex>>
	void SubmitQuads(
		PipelineId pipeline_id, RenderTargetId target, const MaterialState& material,
		const RenderState& render_state, std::span<const RenderQuad<TVertex>> quads,
		std::span<const TextureId> local_textures = {}, TAccessor texture_index = {}
	) {
		SubmitPrimitives<TVertex, 4, 6>(
			pipeline_id, target, material, render_state, quads, kQuadIndices, local_textures,
			texture_index
		);
	}

	template <VertexType TVertex, typename TAccessor = DefaultTextureIndexAccessor<TVertex>>
	void SubmitTriangles(
		PipelineId pipeline_id, RenderTargetId target, const MaterialState& material,
		const RenderState& render_state, std::span<const RenderTriangle<TVertex>> triangles,
		std::span<const TextureId> local_textures = {}, TAccessor texture_index = {}
	) {
		SubmitPrimitives<TVertex, 3, 3>(
			pipeline_id, target, material, render_state, triangles, kTriangleIndices,
			local_textures, texture_index
		);
	}

private:
	void EnsureActiveBatchState(
		PipelineId pipeline_id, RenderTargetId target, const MaterialState& material,
		const RenderState& render_state
	);

	struct TextureSlotInfo {
		std::uint32_t slot{ 0 };
		bool push_to_batch{ false };
	};

	TextureSlotInfo GetTextureSlotNoFlush(TextureId texture) const;

	template <
		VertexType TVertex, std::size_t VertexCount, std::size_t IndexCount, typename TPrimitive,
		typename TAccessor>
	void SubmitPrimitives(
		PipelineId pipeline_id, RenderTargetId target, const MaterialState& material,
		const RenderState& render_state, std::span<const TPrimitive> primitives,
		const std::array<Index, IndexCount>& index_pattern,
		std::span<const TextureId> local_textures, TAccessor texture_index
	) {
		static_assert(std::is_trivially_copyable_v<TVertex>);
		static_assert(std::is_standard_layout_v<TVertex>);
		static_assert(std::tuple_size_v<TPrimitive> == VertexCount);

		if (primitives.empty()) {
			return;
		}

		EnsureActiveBatchState(pipeline_id, target, material, render_state);

		std::vector<TVertex> chunk_vertices;
		std::vector<Index> chunk_indices;

		chunk_vertices.reserve(
			std::min<std::size_t>(primitives.size() * VertexCount, kVertexCapacity)
		);

		chunk_indices.reserve(std::min<std::size_t>(primitives.size() * IndexCount, kIndexCapacity)
		);

		auto flush_chunk = [&]() {
			if (chunk_vertices.empty()) {
				return;
			}

			SubmitVertices<TVertex>(pipeline_id, chunk_vertices, chunk_indices);

			chunk_vertices.clear();
			chunk_indices.clear();
		};

		for (const TPrimitive& primitive : primitives) {
			TPrimitive copied = primitive;

			float batch_texture_slot = 0.0f;

			if (!local_textures.empty()) {
				const auto local_texture_index = static_cast<std::size_t>(texture_index(copied[0]));

				PTGN_ASSERT(
					local_texture_index < local_textures.size(), "Invalid local texture index"
				);

				const TextureId texture = local_textures[local_texture_index];

				PTGN_ASSERT(texture != 0);

				if (IsTextureAttachedToCurrentFramebuffer(texture)) {
					PTGN_ERROR("Cannot sample from a texture attached to the current framebuffer");
				}

				const bool already_bound = std::ranges::contains(textures_, texture);

				if (!already_bound && textures_.size() >= GetMaxTextureSlots()) {
					flush_chunk();
					Flush();

					// Flush keeps active pipeline/material/state/target, so we can continue.
					EnsureActiveBatchState(pipeline_id, target, material, render_state);
				}

				TextureSlotInfo slot = GetTextureSlotNoFlush(texture);

				if (slot.push_to_batch) {
					textures_.push_back(texture);
				}

				batch_texture_slot = static_cast<float>(slot.slot);

				for (TVertex& vertex : copied) {
					texture_index(vertex) = batch_texture_slot;
				}
			}

			if (ChunkExceedsCapacity<TVertex>(
					pipeline_id, chunk_vertices.size() + VertexCount,
					chunk_indices.size() + IndexCount
				)) {
				flush_chunk();
			}

			const auto base_vertex = static_cast<Index>(chunk_vertices.size());

			for (const TVertex& vertex : copied) {
				chunk_vertices.push_back(vertex);
			}

			for (Index index : index_pattern) {
				chunk_indices.push_back(base_vertex + index);
			}
		}

		flush_chunk();
	}

	template <VertexType TVertex>
	void SubmitVertices(
		PipelineId pipeline_id, std::span<const TVertex> vertices,
		std::span<const Index> local_indices
	) {
		const RenderPipeline& pipeline = GetPipeline(pipeline_id);

		const std::size_t vertex_bytes = vertices.size() * sizeof(TVertex);
		const std::size_t vertex_capacity_bytes =
			static_cast<std::size_t>(pipeline.vertex_capacity) * pipeline.vertex_size;

		PTGN_ASSERT(vertex_bytes <= vertex_capacity_bytes, "Single submit exceeds vertex capacity");

		PTGN_ASSERT(
			local_indices.size() <= pipeline.index_capacity, "Single submit exceeds index capacity"
		);

		if (vertices_.size() + vertex_bytes > vertex_capacity_bytes ||
			indices_.size() + local_indices.size() > pipeline.index_capacity) {
			Flush();
		}

		const auto base_vertex = static_cast<Index>(vertices_.size() / sizeof(TVertex));

		const auto bytes = std::as_bytes(vertices);

		vertices_.insert(vertices_.end(), bytes.begin(), bytes.end());

		indices_.reserve(indices_.size() + local_indices.size());

		for (Index index : local_indices) {
			indices_.push_back(base_vertex + index);
		}
	}

	template <VertexType TVertex>
	bool ChunkExceedsCapacity(
		PipelineId pipeline_id, std::size_t vertex_count, std::size_t index_count
	) const {
		const RenderPipeline& pipeline = GetPipeline(pipeline_id);

		return vertex_count > pipeline.vertex_capacity || index_count > pipeline.index_capacity;
	}

	void ReleaseTargetsAfterFlush();

	[[nodiscard]] bool IsTextureAttachedToCurrentFramebuffer(TextureId texture) const;

	std::size_t GetMaxTextureSlots() const;

	const RenderPipeline& GetPipeline(std::size_t id) const;

	RenderPipeline& GetPipeline(std::size_t id);

	Renderer& renderer_;

	std::optional<PipelineId> active_pipeline_id_;
	RenderTargetId active_target_{ 0 };
	MaterialState active_material_;
	RenderState active_render_state_;

	std::vector<std::byte> vertices_;
	std::vector<Index> indices_;
	std::vector<TextureId> textures_;

	std::vector<const RenderTargetObject*> release_after_flush_;
};

} // namespace ptgn::impl
