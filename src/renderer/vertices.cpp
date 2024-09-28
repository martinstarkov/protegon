#include "renderer/vertices.h"

#include <array>
#include <cmath>

#include "protegon/math.h"
#include "protegon/vector2.h"
#include "protegon/vector4.h"
#include "renderer/origin.h"
#include "utility/debug.h"

namespace ptgn::impl {

void OffsetVertices(std::array<V2_float, 4>& vertices, const V2_float& size, Origin draw_origin) {
	auto draw_offset = GetOffsetFromCenter(size, draw_origin);

	// Offset each vertex by based on draw origin.
	if (!draw_offset.IsZero()) {
		for (auto& v : vertices) {
			v -= draw_offset;
		}
	}
}

void RotateVertices(
	std::array<V2_float, 4>& vertices, const V2_float& position, const V2_float& size,
	float rotation_radians, const V2_float& rotation_center
) {
	PTGN_ASSERT(
		rotation_center.x >= 0.0f && rotation_center.x <= 1.0f,
		"Rotation center must be within 0.0f and 1.0f"
	);
	PTGN_ASSERT(
		rotation_center.y >= 0.0f && rotation_center.y <= 1.0f,
		"Rotation center must be within 0.0f and 1.0f"
	);

	V2_float half{ size * 0.5f };

	V2_float rot{ -size * rotation_center };

	V2_float s0{ rot };
	V2_float s1{ size.x + rot.x, rot.y };
	V2_float s2{ size + rot };
	V2_float s3{ rot.x, size.y + rot.y };

	float c{ 1.0f };
	float s{ 0.0f };

	if (!NearlyEqual(rotation_radians, 0.0f)) {
		c = std::cos(rotation_radians);
		s = std::sin(rotation_radians);
	}

	auto rotated = [&](const V2_float& coordinate) {
		return position - rot - half +
			   V2_float{ c * coordinate.x - s * coordinate.y, s * coordinate.x + c * coordinate.y };
	};

	vertices[0] = rotated(s0);
	vertices[1] = rotated(s1);
	vertices[2] = rotated(s2);
	vertices[3] = rotated(s3);
}

std::array<V2_float, 4> GetQuadVertices(
	const V2_float& position, const V2_float& size, Origin draw_origin, float rotation_radians,
	const V2_float& rotation_center
) {
	std::array<V2_float, 4> vertices;

	RotateVertices(vertices, position, size, rotation_radians, rotation_center);
	OffsetVertices(vertices, size, draw_origin);

	return vertices;
}

QuadVertices::QuadVertices(
	const std::array<V2_float, 4>& vertices, float z_index, const V4_float& color,
	const std::array<V2_float, 4>& tex_coords, float texture_index
) :
	ShapeVertices{ vertices, z_index, color } {
	for (std::size_t i{ 0 }; i < vertices_.size(); i++) {
		vertices_[i].tex_coord = { tex_coords[i].x, tex_coords[i].y };
		vertices_[i].tex_index = { texture_index };
	}
}

CircleVertices::CircleVertices(
	const std::array<V2_float, 4>& vertices, float z_index, const V4_float& color, float line_width,
	float fade
) :
	ShapeVertices{ vertices, z_index, color } {
	constexpr std::array<V2_float, 4> local{
		V2_float{ -1.0f, -1.0f },
		V2_float{ 1.0f, -1.0f },
		V2_float{ 1.0f, 1.0f },
		V2_float{ -1.0f, 1.0f },
	};
	for (std::size_t i{ 0 }; i < vertices_.size(); i++) {
		vertices_[i].local_position = { local[i].x, local[i].y, 0.0f };
		vertices_[i].line_width		= { line_width };
		vertices_[i].fade			= { fade };
	}
}

} // namespace ptgn::impl