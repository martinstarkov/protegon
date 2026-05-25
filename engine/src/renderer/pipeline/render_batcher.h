#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
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

inline constexpr std::uint32_t kBatchCapacity{ 10000 };
inline constexpr std::uint32_t kVertexCapacity{ kBatchCapacity * 4 };
inline constexpr std::uint32_t kIndexCapacity{ kBatchCapacity * 6 };

class RenderBatcher {
public:
	explicit RenderBatcher(Renderer& renderer);

	void Flush();

	void HoldUntilFlush(RenderTargetObject target);

	template <VertexType TVertex>
	void SubmitQuads(
		std::span<RenderQuad<TVertex>> quads, std::size_t vertex_capacity,
		std::size_t index_capacity, std::span<const TextureId> local_textures = {}
	) {
		SubmitPrimitives<TVertex, std::tuple_size_v<RenderQuad<TVertex>>, kQuadIndices.size()>(
			quads, kQuadIndices, local_textures, vertex_capacity, index_capacity
		);
	}

	template <VertexType TVertex>
	void SubmitTriangles(
		std::span<RenderTriangle<TVertex>> triangles, std::size_t vertex_capacity,
		std::size_t index_capacity, std::span<const TextureId> local_textures = {}
	) {
		SubmitPrimitives<
			TVertex, std::tuple_size_v<RenderTriangle<TVertex>>, kTriangleIndices.size()>(
			triangles, kTriangleIndices, local_textures, vertex_capacity, index_capacity
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
		VertexType TVertex, std::size_t VertexCount, std::size_t IndexCount, typename TPrimitive>
	void SubmitPrimitives(
		std::span<TPrimitive> primitives, const std::array<Index, IndexCount>& index_pattern,
		std::span<const TextureId> local_textures, std::size_t vertex_capacity,
		std::size_t index_capacity
	) {
		static_assert(std::is_standard_layout_v<TVertex>);
		static_assert(std::tuple_size_v<TPrimitive> == VertexCount);

		static_assert(std::ranges::contiguous_range<TPrimitive>);
		static_assert(std::same_as<std::ranges::range_value_t<TPrimitive>, TVertex>);

		if (primitives.empty()) {
			return;
		}

		PTGN_ASSERT(VertexCount <= vertex_capacity, "Single primitive exceeds vertex capacity");
		PTGN_ASSERT(IndexCount <= index_capacity, "Single primitive exceeds index capacity");

		for (auto& primitive : primitives) {
			if (BatchWouldExceedCapacity<TVertex>(
					VertexCount, IndexCount, vertex_capacity, index_capacity
				)) {
				Flush();
			}

			auto batch_texture_slot{
				ResolveTextureSlotForPrimitive<TVertex>(primitive, local_textures)
			};

			if constexpr (TextureIndexAccessor<TVertex>::has_texture_index) {
				if (!local_textures.empty()) {
					for (auto& vertex : primitive) {
						TextureIndexAccessor<TVertex>::Get(vertex) = batch_texture_slot;
					}
				}
			}

			SubmitPrimitiveUnchecked<TVertex, VertexCount, IndexCount>(primitive, index_pattern);
		}
	}

	template <VertexType TVertex, typename TPrimitive>
	float ResolveTextureSlotForPrimitive(
		TPrimitive& primitive, std::span<const TextureId> local_textures
	) {
		if (local_textures.empty()) {
			return 0.0f;
		}

		if constexpr (TextureIndexAccessor<TVertex>::has_texture_index) {
			auto local_texture_index{
				static_cast<std::size_t>(TextureIndexAccessor<TVertex>::Get(primitive[0]))
			};

			PTGN_ASSERT(local_texture_index < local_textures.size(), "Invalid local texture index");

			TextureId texture{ local_textures[local_texture_index] };

			PTGN_ASSERT(texture);

			if (IsTextureAttachedToCurrentFramebuffer(texture)) {
				PTGN_ERROR("Cannot sample from a texture attached to the current framebuffer");
			}

			if (bool already_bound{ std::ranges::contains(textures_, texture) };
				!already_bound && textures_.size() >= GetMaxTextureSlots()) {
				Flush();
			}

			auto slot{ GetTextureSlotNoFlush(texture) };

			if (slot.push_to_batch) {
				textures_.push_back(texture);
			}

			return static_cast<float>(slot.slot);
		} else {
			PTGN_ERROR("Vertex type must have a texture index when submitting with textures");
		}
	}

	template <VertexType TVertex>
	bool BatchWouldExceedCapacity(
		std::size_t additional_vertex_count, std::size_t additional_index_count,
		std::size_t vertex_capacity, std::size_t index_capacity
	) const {
		auto additional_vertex_bytes{ additional_vertex_count * sizeof(TVertex) };
		auto vertex_capacity_bytes{ vertex_capacity * sizeof(TVertex) };

		return vertices_.size() + additional_vertex_bytes > vertex_capacity_bytes ||
			   indices_.size() + additional_index_count > index_capacity;
	}

	template <
		VertexType TVertex, std::size_t VertexCount, std::size_t IndexCount, typename TPrimitive>
	void SubmitPrimitiveUnchecked(
		const TPrimitive& primitive, const std::array<Index, IndexCount>& index_pattern
	) {
		auto base_vertex{ static_cast<Index>(vertices_.size() / sizeof(TVertex)) };

		std::span<const TVertex, VertexCount> primitive_vertices{ std::data(primitive),
																  VertexCount };

		auto bytes{ std::as_bytes(primitive_vertices) };

		PTGN_ASSERT(
			vertex_size_ == 0 || vertex_size_ == sizeof(TVertex),
			"Inconsistent vertex size in batch"
		);

		vertex_size_ = sizeof(TVertex);

		vertices_.append_range(bytes);

		indices_.append_range(index_pattern | std::views::transform([base_vertex](auto index) {
								  return base_vertex + index;
							  }));
	}

	void ReleaseTargetsAfterFlush();

	[[nodiscard]] bool IsTextureAttachedToCurrentFramebuffer(TextureId texture) const;

	std::size_t GetMaxTextureSlots() const;

	Renderer& renderer_;

	std::uint32_t vertex_size_{ 0 };
	std::vector<std::byte> vertices_;
	std::vector<Index> indices_;
	std::vector<TextureId> textures_;

	std::vector<RenderTargetObject> release_after_flush_;
};

} // namespace ptgn::impl
