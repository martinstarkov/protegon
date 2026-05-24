#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/vertex.h"
#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"

namespace ptgn::impl {

class Renderer;

using Index = std::uint32_t;

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

inline constexpr std::uint32_t kBatchCapacity{ 10000 };
inline constexpr std::uint32_t kVertexCapacity{ kBatchCapacity * 4 };
inline constexpr std::uint32_t kIndexCapacity{ kBatchCapacity * 6 };

class RenderBatcher {
public:
	explicit RenderBatcher(Renderer& renderer);

	void Flush();

	void HoldUntilFlush(RenderTargetObject target);

	template <VertexType TVertex, typename TAccessor = DefaultTextureIndexAccessor<TVertex>>
	void SubmitQuads(
		std::span<const RenderQuad<TVertex>> quads, std::size_t vertex_capacity,
		std::size_t index_capacity, std::size_t vertex_size,
		std::span<const TextureId> local_textures = {}, TAccessor texture_index = {}
	) {
		SubmitPrimitives<TVertex, std::tuple_size_v<RenderQuad<TVertex>>, kQuadIndices.size()>(
			quads, kQuadIndices, local_textures, texture_index, vertex_capacity, index_capacity,
			vertex_size
		);
	}

	template <VertexType TVertex, typename TAccessor = DefaultTextureIndexAccessor<TVertex>>
	void SubmitTriangles(
		std::span<const RenderTriangle<TVertex>> triangles, std::size_t vertex_capacity,
		std::size_t index_capacity, std::size_t vertex_size,
		std::span<const TextureId> local_textures = {}, TAccessor texture_index = {}
	) {
		SubmitPrimitives<
			TVertex, std::tuple_size_v<RenderTriangle<TVertex>>, kTriangleIndices.size()>(
			triangles, kTriangleIndices, local_textures, texture_index, vertex_capacity,
			index_capacity, vertex_size
		);
	}

private:
	friend class Renderer;

	struct TextureSlotInfo {
		std::uint32_t slot{ 0 };
		bool push_to_batch{ false };
	};

	TextureSlotInfo GetTextureSlotNoFlush(TextureId texture) const;

	template <
		VertexType TVertex, std::size_t VertexCount, std::size_t IndexCount, typename TPrimitive,
		typename TAccessor>
	void SubmitPrimitives(
		std::span<const TPrimitive> primitives, const std::array<Index, IndexCount>& index_pattern,
		std::span<const TextureId> local_textures, TAccessor texture_index,
		std::size_t vertex_capacity, std::size_t index_capacity, std::size_t vertex_size
	) {
		static_assert(std::is_trivially_copyable_v<TVertex>);
		static_assert(std::is_standard_layout_v<TVertex>);
		static_assert(std::tuple_size_v<TPrimitive> == VertexCount);

		if (primitives.empty()) {
			return;
		}

		std::vector<TVertex> chunk_vertices;
		std::vector<Index> chunk_indices;

		chunk_vertices.reserve(
			std::min<std::size_t>(primitives.size() * VertexCount, kVertexCapacity)
		);

		chunk_indices.reserve(
			std::min<std::size_t>(primitives.size() * IndexCount, kIndexCapacity)
		);

		auto flush_chunk = [&]() {
			if (chunk_vertices.empty()) {
				return;
			}

			SubmitVertices<TVertex>(
				chunk_vertices, chunk_indices, vertex_capacity, index_capacity, vertex_size
			);

			chunk_vertices.clear();
			chunk_indices.clear();
		};

		for (const auto& primitive : primitives) {
			TPrimitive copied{ primitive };

			auto batch_texture_slot{ 0.0f };

			if (!local_textures.empty()) {
				auto local_texture_index{ static_cast<std::size_t>(texture_index(copied[0])) };

				PTGN_ASSERT(
					local_texture_index < local_textures.size(), "Invalid local texture index"
				);

				TextureId texture{ local_textures[local_texture_index] };

				PTGN_ASSERT(texture);

				if (IsTextureAttachedToCurrentFramebuffer(texture)) {
					PTGN_ERROR("Cannot sample from a texture attached to the current framebuffer");
				}

				if (bool already_bound{ std::ranges::contains(textures_, texture) };
					!already_bound && textures_.size() >= GetMaxTextureSlots()) {
					flush_chunk();
					Flush();
				}

				auto slot{ GetTextureSlotNoFlush(texture) };

				if (slot.push_to_batch) {
					textures_.push_back(texture);
				}

				batch_texture_slot = static_cast<float>(slot.slot);

				for (auto& vertex : copied) {
					texture_index(vertex) = batch_texture_slot;
				}
			}

			if (ChunkExceedsCapacity<TVertex>(
					chunk_vertices.size() + VertexCount, chunk_indices.size() + IndexCount,
					vertex_capacity, index_capacity
				)) {
				flush_chunk();
			}

			auto base_vertex{ static_cast<Index>(chunk_vertices.size()) };

			for (const auto& vertex : copied) {
				chunk_vertices.push_back(vertex);
			}

			for (auto index : index_pattern) {
				chunk_indices.push_back(base_vertex + index);
			}
		}

		flush_chunk();
	}

	template <VertexType TVertex>
	void SubmitVertices(
		std::span<const TVertex> vertices, std::span<const Index> local_indices,
		std::size_t vertex_capacity, std::size_t index_capacity, std::size_t vertex_size
	) {
		std::size_t vertex_bytes{ vertices.size() * sizeof(TVertex) };
		std::size_t vertex_capacity_bytes{ vertex_capacity * vertex_size };

		PTGN_ASSERT(vertex_bytes <= vertex_capacity_bytes, "Single submit exceeds vertex capacity");

		PTGN_ASSERT(local_indices.size() <= index_capacity, "Single submit exceeds index capacity");

		if (vertices_.size() + vertex_bytes > vertex_capacity_bytes ||
			indices_.size() + local_indices.size() > index_capacity) {
			Flush();
		}

		auto base_vertex{ static_cast<Index>(vertices_.size() / sizeof(TVertex)) };

		auto bytes{ std::as_bytes(vertices) };

		vertices_.insert(vertices_.end(), bytes.begin(), bytes.end());

		indices_.reserve(indices_.size() + local_indices.size());

		for (auto index : local_indices) {
			indices_.push_back(base_vertex + index);
		}
	}

	template <VertexType TVertex>
	bool ChunkExceedsCapacity(
		std::size_t vertex_count, std::size_t index_count, std::size_t vertex_capacity,
		std::size_t index_capacity
	) const {
		return vertex_count > vertex_capacity || index_count > index_capacity;
	}

	void ReleaseTargetsAfterFlush();

	[[nodiscard]] bool IsTextureAttachedToCurrentFramebuffer(TextureId texture) const;

	std::size_t GetMaxTextureSlots() const;

	Renderer& renderer_;

	std::vector<std::byte> vertices_;
	std::vector<Index> indices_;
	std::vector<TextureId> textures_;

	std::vector<RenderTargetObject> release_after_flush_;
};

} // namespace ptgn::impl
