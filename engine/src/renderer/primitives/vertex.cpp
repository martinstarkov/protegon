#include "renderer/primitives/vertex.h"

#include <array>
#include <ostream>
#include <utility>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/flip.h"

namespace ptgn {

namespace impl {

std::array<V2_float, 4> GetTextureCoordinates(
	V2_float source_position, V2_float source_size, V2_float texture_size, bool flip_vertically,
	bool offset_texels
) {
	PTGN_ASSERT(texture_size.x > 0.0f, "Texture must have width > 0");
	PTGN_ASSERT(texture_size.y > 0.0f, "Texture must have height > 0");

	PTGN_ASSERT(
		source_position.x < texture_size.x, "Source position X must be within texture width"
	);
	PTGN_ASSERT(
		source_position.y < texture_size.y, "Source position Y must be within texture height"
	);

	if (source_size.IsZero()) {
		source_size = texture_size - source_position;
	}

	V2_float texel = offset_texels ? (0.5f / texture_size) : V2_float{ 0, 0 };

	V2_float min = (source_position - texel) / texture_size;
	V2_float max = (source_position + source_size - texel) / texture_size;

	if (max.x > 1.0f || max.y > 1.0f) {
		PTGN_WARN("Drawing source size from outside of texture size");
	}

	std::array<V2_float, 4> uv{ min, { max.x, min.y }, max, { min.x, max.y } };

	if (flip_vertically) {
		FlipTextureCoordinates(uv, Flip::Vertical);
	}

	return uv;
}

void FlipTextureCoordinates(std::array<V2_float, 4>& tex_coords, V2_float scale) {
	bool flip_x{ scale.x < 0.0f };
	bool flip_y{ scale.y < 0.0f };

	if (flip_x && flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Both);
	} else if (flip_x) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Horizontal);
	} else if (flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
	}
}

void FlipTextureCoordinates(std::array<V2_float, 4>& tex_coords, Flip flip) {
	const auto flip_x = [&]() {
		std::swap(tex_coords[0].x, tex_coords[1].x);
		std::swap(tex_coords[2].x, tex_coords[3].x);
	};
	const auto flip_y = [&]() {
		std::swap(tex_coords[0].y, tex_coords[3].y);
		std::swap(tex_coords[1].y, tex_coords[2].y);
	};
	switch (flip) {
		case Flip::None:	   break;
		case Flip::Vertical:   flip_y(); break;
		case Flip::Horizontal: flip_x(); break;
		case Flip::Both:
			flip_x();
			flip_y();
			break;
		default: PTGN_ERROR("Unrecognized flip state");
	}
}

std::array<V2_float, 4> GetCenteredQuadPoints(V2_float size) {
	V2_float half{ size / 2.0f };
	return { -half, V2_float{ half.x, -half.y }, half, V2_float{ -half.x, half.y } };
}

std::array<Vertex, 2> Vertex::GetLine(
	const std::array<V2_float, 2>& line_points, Color color, float depth
) {
	constexpr std::array<V2_float, 2> line_coordinates{ V2_float{ 0.0f, 0.0f },
														V2_float{ 1.0f, 0.0f } };

	std::array<Vertex, 2> vertices{};

	auto c{ color.Normalized() };

	PTGN_ASSERT(vertices.size() == line_points.size());
	PTGN_ASSERT(vertices.size() == line_coordinates.size());

	for (std::size_t i{ 0 }; i < line_points.size(); i++) {
		vertices[i].position  = { line_points[i].x, line_points[i].y, depth };
		vertices[i].color	  = { c.x, c.y, c.z, c.w };
		vertices[i].tex_coord = { line_coordinates[i].x, line_coordinates[i].y };
	}

	return vertices;
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
	const std::array<float, 4>& data, std::array<V2_float, 4> texture_coordinates
) {
	std::array<Vertex, 4> vertices{};

	auto c{ color.Normalized() };

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

} // namespace impl

template <typename T, std::size_t N>
std::ostream& operator<<(std::ostream& os, const std::array<T, N>& arr) {
	os << "(";
	for (std::size_t i = 0; i < N; ++i) {
		os << arr[i];
		if (i + 1 < N) {
			os << ", ";
		}
	}
	os << ")";
	return os;
}

std::ostream& operator<<(std::ostream& os, const impl::Vertex& v) {
	os << "Vertex{ ";
	os << "pos=" << v.position;
	os << ", col=" << v.color;
	os << ", uv=" << v.tex_coord;
	os << ", data=" << v.data;
	os << " }";
	return os;
}

} // namespace ptgn