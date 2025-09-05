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
	// Index 0: For textures this is from 1 to max_texture_slots.
	// Index 0: For solid triangles/quads this is 0 (white 1x1 texture).
	// Index 0: For circles this stores the thickness: 0 is hollow, 1 is solid.
	glsl::vec4 data{};

	static constexpr BufferLayout<glsl::vec3, glsl::vec4, glsl::vec2, glsl::vec4> GetLayout() {
		return {};
	}

	[[nodiscard]] static std::array<Vertex, 3> GetTriangle(
		const std::array<V2_float, 3>& triangle_points, const Color& color, const Depth& depth
	);

	[[nodiscard]] static std::array<Vertex, 4> GetQuad(
		const std::array<V2_float, 4>& quad_points, const Color& color, const Depth& depth,
		const std::array<float, 4>& data, std::array<V2_float, 4> texture_coordinates,
		bool flip_vertices = false
	);

	[[nodiscard]] static void SetTextureIndex(std::array<Vertex, 4>& vertices, float texture_index);
};

} // namespace impl

} // namespace ptgn