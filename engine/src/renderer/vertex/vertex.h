#pragma once

#include <array>

#include "core/graphics/flip.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "core/util/concepts.h"
#include "renderer/pipeline/buffer_layout.h"
#include "renderer/vertex/glsl_types.h"

namespace ptgn {

struct Color;
struct Depth;

namespace impl {

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

	glsl::vec3 position{};
	glsl::vec4 color{};
	glsl::vec2 tex_coord{};
	glsl::float_ tex_index{};
	glsl::int_ entity_id{ -1 };
};

template <VertexType T, std::size_t I, InvocableR<T, std::size_t> F>
std::array<T, I> GetVertices(F&& transform) {
	std::array<T, I> vertices{};
	for (auto i{ 0uz }; i < I; ++i) {
		vertices[i] = std::invoke(std::forward<F>(transform), i);
	}
	return vertices;
}

std::array<V2_float, 4> GetCenteredQuadPoints(V2_float size);

} // namespace impl

} // namespace ptgn