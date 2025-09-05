#pragma once

#include <array>

#include "math/vector2.h"
#include "renderer/buffers/buffer_layout.h"
#include "renderer/gl/gl_types.h"

namespace ptgn {

struct Color;
struct Depth;

namespace impl {

struct Vertex {
	glsl::vec3 position{};
	glsl::vec4 color{};
	glsl::vec2 tex_coord{};
	// For textures this is from 1 to max_texture_slots.
	// For solid triangles/quads this is 0 (white 1x1 texture).
	// For circles this stores the thickness: 0 is hollow, 1 is solid.
	glsl::float_ tex_index{};

	static constexpr BufferLayout<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_> GetLayout() {
		return {};
	}

	[[nodiscard]] static std::array<Vertex, 3> GetTriangle(
		const std::array<V2_float, 3>& triangle_points, const Color& color, const Depth& depth
	);

	[[nodiscard]] static std::array<Vertex, 4> GetQuad(
		const std::array<V2_float, 4>& quad_points, const Color& color, const Depth& depth,
		float texture_index, std::array<V2_float, 4> texture_coordinates, bool flip_vertices = false
	);
};

} // namespace impl

} // namespace ptgn