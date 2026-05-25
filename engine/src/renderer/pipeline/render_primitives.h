#pragma once

#include <array>
#include <type_traits>

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

[[nodiscard]] RenderTriangle<ColorVertex> CreateColorTriangle(
	const std::array<V2_float, 3>& vertices, float depth, V4_float color_n, int entity_id
);

[[nodiscard]] RenderQuad<ColorVertex> CreateColorQuad(
	const std::array<V2_float, 4>& vertices, float depth, V4_float color_n, int entity_id
);

[[nodiscard]] RenderQuad<ShapeVertex> CreateShapeQuad(
	const std::array<V2_float, 4>& vertices, float depth, V4_float color_n,
	const std::array<V2_float, 4>& local_coords, const std::array<float, 4>& shape_data,
	int entity_id
);

[[nodiscard]] RenderQuad<TextureVertex> CreateTextureQuad(
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

} // namespace ptgn::impl