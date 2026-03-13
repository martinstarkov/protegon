#pragma once

#include <array>
#include <ostream>

#include "core/math/vector2.h"
#include "renderer/primitives/buffer_layout.h"
#include "renderer/primitives/flip.h"
#include "renderer/primitives/glsl_types.h"

namespace ptgn {

struct Color;
struct Depth;

namespace impl {

template <bool kFlipY>
[[nodiscard]] inline constexpr std::array<V2_float, 4> GetDefaultTextureCoordinates() {
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

[[nodiscard]] inline constexpr std::array<V2_float, 4> GetDefaultTextureCoordinates(bool flip_y) {
	if (flip_y) {
		return GetDefaultTextureCoordinates<true>();
	} else {
		return GetDefaultTextureCoordinates<false>();
	}
}

[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(
	V2_float source_position, V2_float source_size, V2_float texture_size, bool flip_vertically,
	bool offset_texels
);

void FlipTextureCoordinates(std::array<V2_float, 4>& tex_coords, V2_float scale);
void FlipTextureCoordinates(std::array<V2_float, 4>& tex_coords, Flip flip);

struct Vertex : public gl::VertexLayout<Vertex, glsl::vec3, glsl::vec4, glsl::vec2, glsl::vec4> {
	glsl::vec3 position{};
	glsl::vec4 color{};
	glsl::vec2 tex_coord{};
	// Index 0: For textures this is from 1 to max_texture_slots.
	// Index 0: For solid triangles/quads this is 0 (white 1x1 texture).
	// Index 0: For circles this stores the thickness: 0 is hollow, 1 is solid.
	glsl::vec4 data{};

	[[nodiscard]] static std::array<Vertex, 2> GetLine(
		const std::array<V2_float, 2>& line_points, Color color, float depth
	);

	[[nodiscard]] static std::array<Vertex, 3> GetTriangle(
		const std::array<V2_float, 3>& triangle_points, Color color, float depth
	);

	[[nodiscard]] static std::array<Vertex, 4> GetQuad(
		const std::array<V2_float, 4>& quad_points, Color color, float depth,
		const std::array<float, 4>& data, std::array<V2_float, 4> tex_coords
	);

	static void SetTextureIndex(std::array<Vertex, 4>& vertices, float texture_index);
};

std::array<V2_float, 4> GetCenteredQuadPoints(V2_float size);

} // namespace impl

std::ostream& operator<<(std::ostream& os, const impl::Vertex& v);

} // namespace ptgn