#pragma once

#include <array>
#include <ranges>
#include <span>
#include <type_traits>

#include "core/assert.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "core/util/concepts.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/vertex.h"

namespace ptgn::impl {

template <VertexType TVertex>
using RenderQuad = std::array<TVertex, 4>;

template <VertexType TVertex>
using RenderTriangle = std::array<TVertex, 3>;

using ColorTriangle = RenderTriangle<ColorVertex>;
using ColorQuad		= RenderQuad<ColorVertex>;
using ShapeQuad		= RenderQuad<ShapeVertex>;
using TextureQuad	= RenderQuad<TextureVertex>;

[[nodiscard]] ColorTriangle CreateColorTriangle(
	const std::array<V2_float, 3>& vertices, float depth, V4_float color_n, int entity_id
);

[[nodiscard]] ColorQuad CreateColorQuad(
	const std::array<V2_float, 4>& vertices, float depth, V4_float color_n, int entity_id
);

[[nodiscard]] ShapeQuad CreateShapeQuad(
	const std::array<V2_float, 4>& vertices, float depth, V4_float color_n,
	const std::array<V2_float, 4>& local_coords, const std::array<float, 4>& shape_data,
	int entity_id
);

[[nodiscard]] TextureQuad CreateTextureQuad(
	const std::array<V2_float, 4>& positions, float depth, V4_float color_n,
	const std::array<V2_float, 4>& tex_coords, int entity_id
);

template <typename T>
struct RenderPrimitiveInfo {
	static constexpr bool valid{ false };
};

template <VertexType TVertex>
	requires(!IsAnyOf<TVertex, TextureVertex, ShapeVertex>)
struct RenderPrimitiveInfo<RenderTriangle<TVertex>> {
	static constexpr bool valid{ true };
	static constexpr std::size_t vertex_count{ 3 };

	using Vertex = TVertex;
};

template <VertexType TVertex>
struct RenderPrimitiveInfo<RenderQuad<TVertex>> {
	static constexpr bool valid{ true };
	static constexpr std::size_t vertex_count{ 4 };

	using Vertex = TVertex;
};

template <typename T>
concept RenderPrimitive = RenderPrimitiveInfo<std::remove_cvref_t<T>>::valid;

template <RenderPrimitive T>
void ApplyTransform(Transform transform, std::span<T> primitives) {
	using TVertex = typename RenderPrimitiveInfo<std::remove_cvref_t<T>>::Vertex;

	if (transform.IsIdentity()) {
		return;
	}

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

} // namespace ptgn::impl