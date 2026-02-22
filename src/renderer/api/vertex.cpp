#include "renderer/api/vertex.h"

#include <array>

#include "common/assert.h"
#include "components/draw.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/flip.h"
#include "renderer/texture.h"

namespace ptgn::impl {

std::array<Vertex, 3> Vertex::GetTriangle(
	const std::array<V2_float, 3>& triangle_points, const Color& color, const Depth& depth
) {
	constexpr std::array<V2_float, 3> texture_coordinates{
		V2_float{ 0.0f, 0.0f }, // lower-left corner
		V2_float{ 1.0f, 0.0f }, // lower-right corner
		V2_float{ 0.5f, 1.0f }, // top-center corner
	};

	std::array<Vertex, 3> vertices{};

	auto c{ color.Normalized() };

	PTGN_ASSERT(vertices.size() == triangle_points.size());
	PTGN_ASSERT(vertices.size() == texture_coordinates.size());

	for (std::size_t i{ 0 }; i < triangle_points.size(); i++) {
		vertices[i].position  = { triangle_points[i].x, triangle_points[i].y,
								  static_cast<float>(depth) };
		vertices[i].color	  = { c.x, c.y, c.z, c.w };
		vertices[i].tex_coord = { texture_coordinates[i].x, texture_coordinates[i].y };
	}

	return vertices;
}

std::array<Vertex, 4> Vertex::GetQuad(
	const std::array<V2_float, 4>& quad_points, const Color& color, const Depth& depth,
	const std::array<float, 4>& data, std::array<V2_float, 4> texture_coordinates,
	bool flip_vertices
) {
	std::array<Vertex, 4> vertices{};

	auto c{ color.Normalized() };

	if (flip_vertices) {
		FlipTextureCoordinates(texture_coordinates, Flip::Vertical);
	}

	PTGN_ASSERT(vertices.size() == quad_points.size());
	PTGN_ASSERT(vertices.size() == texture_coordinates.size());

	for (std::size_t i{ 0 }; i < vertices.size(); ++i) {
		vertices[i].position  = { quad_points[i].x, quad_points[i].y, static_cast<float>(depth) };
		vertices[i].color	  = { c.x, c.y, c.z, c.w };
		vertices[i].tex_coord = { texture_coordinates[i].x, texture_coordinates[i].y };
		vertices[i].data	  = data;
	}

	return vertices;
}

void Vertex::SetTextureIndex(std::array<Vertex, 4>& vertices, float texture_index) {
	for (auto& v : vertices) {
		v.data = { texture_index };
	}
}

} // namespace ptgn::impl