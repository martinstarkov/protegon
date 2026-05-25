#pragma once

#include <array>
#include <concepts>
#include <type_traits>

#include "core/graphics/flip.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/pipeline/glsl_types.h"

namespace ptgn {

namespace impl {

template <VertexType TVertex>
using RenderQuad = std::array<TVertex, 4>;

template <VertexType TVertex>
using RenderTriangle = std::array<TVertex, 3>;

template <typename T>
struct RenderPrimitiveInfo {
	static constexpr bool valid{ false };
};

template <VertexType TVertex>
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

template <bool kFlipY>
constexpr std::array<V2_float, 4> GetDefaultTextureCoordinates() {
	if constexpr (kFlipY) {
		return { V2_float{ 0.0f, 1.0f }, V2_float{ 1.0f, 1.0f }, V2_float{ 1.0f, 0.0f },
				 V2_float{ 0.0f, 0.0f } };

	} else {
		return {
			V2_float{ 0.0f, 0.0f },
			V2_float{ 1.0f, 0.0f },
			V2_float{ 1.0f, 1.0f },
			V2_float{ 0.0f, 1.0f },
		};
	}
}

/// @brief Values from [-1, 1]
constexpr std::array<V2_float, 4> GetNDCTextureCoordinates() {
	return { V2_float{ -1.0f, 1.0f }, V2_float{ 1.0f, 1.0f }, V2_float{ 1.0f, -1.0f },
			 V2_float{ -1.0f, -1.0f } };
}

constexpr std::array<V2_float, 4> GetDefaultTextureCoordinates(bool flip_y) {
	if (flip_y) {
		return GetDefaultTextureCoordinates<true>();
	} else {
		return GetDefaultTextureCoordinates<false>();
	}
}

std::array<V2_float, 4> GetTextureCoordinates(
	V2_float source_position, V2_float source_size, V2_float texture_size, bool flip_vertically,
	bool offset_texels
);

void FlipTextureCoordinates(std::array<V2_float, 4>& tex_coords, V2_float scale);
void FlipTextureCoordinates(std::array<V2_float, 4>& tex_coords, Flip flip);

struct ColorVertex : public VertexLayout<ColorVertex, glsl::vec3, glsl::vec4, glsl::int_> {
	ColorVertex() = default;

	ColorVertex(V2_float position, float depth, V4_float color, int entity_id);

	[[nodiscard]] static RenderQuad<ColorVertex> CreateTriangle(
		const std::array<V2_float, 3>& vertices, float depth, V4_float color_n, int entity_id
	);

	[[nodiscard]] static RenderQuad<ColorVertex> CreateQuad(
		const std::array<V2_float, 4>& vertices, float depth, V4_float color_n, int entity_id
	);

	glsl::vec3 position{};
	glsl::vec4 color{};
	glsl::int_ entity_id{ -1 };
};

struct ShapeVertex :
	public VertexLayout<ShapeVertex, glsl::vec3, glsl::vec4, glsl::vec2, glsl::vec4, glsl::int_> {
	ShapeVertex() = default;

	ShapeVertex(
		V2_float position, float depth, V4_float color, V2_float local_coord,
		const std::array<float, 4>& shape_data, int entity_id
	);

	[[nodiscard]] static RenderQuad<ShapeVertex> CreateQuad(
		const std::array<V2_float, 4>& vertices, float depth, V4_float color_n,
		const std::array<V2_float, 4>& local_coords, const std::array<float, 4>& shape_data,
		int entity_id
	);

	glsl::vec3 position{};
	glsl::vec4 color{};
	glsl::vec2 local_coord{};
	/// @brief Shape-specific data
	/// For circle: x = thickness, y = fade
	/// For ellipse: x = thickness, y = fade
	/// For capsule: x = thickness, y = fade, z = normalized_radius
	/// For rounded rect: x = thickness, y = fade, z = normalized_radius, w = aspect_ratio
	/// For arc: x = thickness, y = fade, z = aperture, w = direction (positive = CW, negative =
	/// CCW)
	glsl::vec4 shape_data{};
	glsl::int_ entity_id{ -1 };
};

struct TextureVertex :
	public VertexLayout<
		TextureVertex, glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_, glsl::int_> {
	TextureVertex() = default;

	TextureVertex(
		V2_float position, float depth, V4_float color, V2_float tex_coord, float tex_index,
		int entity_id
	);

	[[nodiscard]] static RenderQuad<TextureVertex> CreateQuad(
		const std::array<V2_float, 4>& positions, float depth, V4_float color_n,
		const std::array<V2_float, 4>& tex_coords, int entity_id
	);

	glsl::vec3 position{};
	glsl::vec4 color{};
	glsl::vec2 tex_coord{};
	glsl::float_ tex_index{};
	glsl::int_ entity_id{ -1 };
};

struct TextureIndexTag {};

template <typename T>
concept GlslFloatRef = requires(T& value) {
	{ value[0] } -> std::same_as<float&>;
};

template <GlslFloatRef T>
constexpr float& TextureIndexFloatRef(T& value) noexcept {
	return value[0];
}

template <typename TVertex>
concept HasRegisteredTextureIndex = requires(TVertex& vertex) {
	{ TextureIndex(TextureIndexTag{}, vertex) } -> std::same_as<float&>;
};

template <typename TVertex>
concept HasConventionalTextureIndex = requires(TVertex& vertex) {
	{ TextureIndexFloatRef(vertex.tex_index) } -> std::same_as<float&>;
};

template <typename TVertex>
struct TextureIndexAccessor {
	static constexpr bool has_texture_index{ impl::HasRegisteredTextureIndex<TVertex> ||
											 impl::HasConventionalTextureIndex<TVertex> };

	static constexpr float& Get(TVertex& vertex) noexcept
		requires has_texture_index
	{
		if constexpr (impl::HasRegisteredTextureIndex<TVertex>) {
			return TextureIndex(TextureIndexTag{}, vertex);
		} else {
			return impl::TextureIndexFloatRef(vertex.tex_index);
		}
	}
};

#define PTGN_TEXTURE_INDEX_MEMBER(VertexType, Member)             \
	friend constexpr float& TextureIndex(                         \
		::ptgn::impl::TextureIndexTag, VertexType& vertex         \
	) noexcept {                                                  \
		return ::ptgn::impl::TextureIndexFloatRef(vertex.Member); \
	}

} // namespace impl

} // namespace ptgn