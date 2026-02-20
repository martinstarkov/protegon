#include "renderer/resources/vertex.h"

#include <array>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/flip.h"
#include "core/math/vector2.h"

namespace ptgn::impl {

std::array<V2_float, 4> GetTextureCoordinates(
	V2_float source_position, V2_float source_size, V2_float texture_size, bool offset_texels
) {
	PTGN_ASSERT(texture_size.x > 0.0f, "Texture must have width > 0");
	PTGN_ASSERT(texture_size.y > 0.0f, "Texture must have height > 0");

	PTGN_ASSERT(
		source_position.x < texture_size.x, "Source position X must be within texture width"
	);
	PTGN_ASSERT(
		source_position.y < texture_size.y, "Source position Y must be within texture height"
	);

	V2_float size{ source_size };

	if (size.IsZero()) {
		size = texture_size - source_position;
	}

	// Convert to 0 -> 1 range.
	V2_float src_pos{ source_position / texture_size };
	V2_float src_size{ size / texture_size };

	if (src_size.x > 1.0f || src_size.y > 1.0f) {
		PTGN_WARN("Drawing source size from outside of texture size");
	}

	V2_float half_pixel{ (offset_texels ? 0.5f : 0.0f) / texture_size };

	std::array<V2_float, 4> texture_coordinates{
		src_pos + half_pixel,
		V2_float{ src_pos.x + src_size.x - half_pixel.x, src_pos.y + half_pixel.y },
		src_pos + src_size - half_pixel,
		V2_float{ src_pos.x + half_pixel.x, src_pos.y + src_size.y - half_pixel.y },
	};

	return texture_coordinates;
}

void FlipTextureCoordinates(std::array<V2_float, 4>& texture_coords, Flip flip) {
	const auto flip_x = [&]() {
		std::swap(texture_coords[0].x, texture_coords[1].x);
		std::swap(texture_coords[2].x, texture_coords[3].x);
	};
	const auto flip_y = [&]() {
		std::swap(texture_coords[0].y, texture_coords[3].y);
		std::swap(texture_coords[1].y, texture_coords[2].y);
	};
	switch (flip) {
		case Flip::None:	   break;
		case Flip::Horizontal: flip_x(); break;
		case Flip::Vertical:   flip_y(); break;
		case Flip::Both:
			flip_x();
			flip_y();
			break;
		default: PTGN_ERROR("Unrecognized flip state");
	}
}

std::array<V2_float, 4> GetCenteredQuadPoints(V2_float size) {
	auto half{ size / 2.0f };
	return { -half, V2_float{ half.x, -half.y }, half, V2_float{ -half.x, half.y } };
}

std::array<Vertex, 3> Vertex::GetTriangle(
	const std::array<V2_float, 3>& triangle_points, Color color, float depth
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
		vertices[i].position  = { triangle_points[i].x, triangle_points[i].y, depth };
		vertices[i].color	  = { c.x, c.y, c.z, c.w };
		vertices[i].tex_coord = { texture_coordinates[i].x, texture_coordinates[i].y };
	}

	return vertices;
}

std::array<Vertex, 4> Vertex::GetQuad(
	const std::array<V2_float, 4>& quad_points, Color color, float depth,
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
		vertices[i].position  = { quad_points[i].x, quad_points[i].y, depth };
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