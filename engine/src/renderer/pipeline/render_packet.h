#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

#include "core/assert.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/render_batch.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"

namespace ptgn {

namespace impl {

struct PrimitiveRange {
	std::uint32_t first_vertex{ 0 };
	std::uint32_t vertex_count{ 0 };

	std::uint32_t first_index{ 0 };
	std::uint32_t index_count{ 0 };

	std::optional<TextureId> texture;
};

struct PrimitiveRequirements {
	std::uint32_t vertex_count{ 0 };
	std::uint32_t index_count{ 0 };
	std::optional<TextureId> texture;
};

template <VertexType TVertex>
struct IndexedPrimitiveGeometry {
	std::vector<TVertex> vertices;
	std::vector<Index> indices;
	std::vector<PrimitiveRange> primitives;
};

template <VertexType TVertex>
struct DefaultTextureIndexAccessor {
	constexpr decltype(auto) operator()(TVertex& vertex) const noexcept {
		if constexpr (requires { vertex.tex_index[0]; }) {
			return (vertex.tex_index[0]);
		} else {
			return (vertex.tex_index);
		}
	}
};

template <typename TAccessor, typename TVertex>
concept TextureIndexAccessorFor =
	VertexType<TVertex> && requires(TAccessor accessor, TVertex& vertex, float texture_index) {
		{ std::invoke(accessor, vertex) } -> std::same_as<float&>;
		std::invoke(accessor, vertex) = texture_index;
	};

struct RenderPacket {
	PipelineId pipeline{ 0 };
	MaterialState material;
	RenderState state_delta;

	std::uint32_t vertex_size{ 0 };

	std::vector<std::byte> vertices;
	std::vector<Index> indices;
	std::vector<PrimitiveRange> primitives;

	std::optional<std::uint32_t> texture_index_offset;
};

inline std::vector<std::byte> CopyVertexBytes(std::span<const std::byte> bytes) {
	return std::vector<std::byte>{ bytes.begin(), bytes.end() };
}

template <VertexType TVertex, typename TAccessor>
	requires TextureIndexAccessorFor<TAccessor, TVertex>
std::uint32_t ComputeTextureIndexOffset(std::span<const TVertex> vertices, TAccessor accessor) {
	PTGN_ASSERT(!vertices.empty(), "Cannot compute texture index offset from empty vertices");

	auto probe{ vertices.front() };

	const auto* base{ reinterpret_cast<const std::byte*>(&probe) };

	auto& texture_index_ref{ std::invoke(accessor, probe) };

	const auto* member{ reinterpret_cast<const std::byte*>(&texture_index_ref) };

	PTGN_ASSERT(member >= base);
	PTGN_ASSERT(member + sizeof(float) <= base + sizeof(TVertex));

	return static_cast<std::uint32_t>(member - base);
}

template <VertexType TVertex>
RenderPacket MakeRenderPacket(
	PipelineId pipeline, MaterialState material, std::span<const TVertex> vertices,
	std::span<const Index> indices, std::span<const PrimitiveRange> primitives
) {
	static_assert(std::is_trivially_copyable_v<TVertex>);
	static_assert(std::is_standard_layout_v<TVertex>);

	auto vertex_bytes{ std::as_bytes(vertices) };

	RenderPacket packet;
	packet.pipeline	   = pipeline;
	packet.material	   = std::move(material);
	packet.vertex_size = sizeof(TVertex);
	packet.vertices	   = CopyVertexBytes(vertex_bytes);
	packet.indices.assign(indices.begin(), indices.end());
	packet.primitives.assign(primitives.begin(), primitives.end());

	return packet;
}

template <VertexType TVertex, typename TAccessor = DefaultTextureIndexAccessor<TVertex>>
	requires TextureIndexAccessorFor<TAccessor, TVertex>
RenderPacket MakeTexturedRenderPacket(
	PipelineId pipeline, MaterialState material, std::span<const TVertex> vertices,
	std::span<const Index> indices, std::span<const PrimitiveRange> primitives,
	TAccessor get_texture_index = {}
) {
	auto packet{ MakeRenderPacket(pipeline, std::move(material), vertices, indices, primitives) };

	packet.texture_index_offset = ComputeTextureIndexOffset(vertices, get_texture_index);

	return packet;
}

} // namespace impl

} // namespace ptgn
