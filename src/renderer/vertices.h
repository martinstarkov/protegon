#pragma once

#include <array>

#include "protegon/vector2.h"
#include "protegon/vector4.h"
#include "renderer/gl_helper.h"
#include "renderer/origin.h"
#include "utility/debug.h"

namespace ptgn::impl {

struct QuadVertex {
	glsl::vec3 position;
	glsl::vec4 color;
	glsl::vec2 tex_coord;
	glsl::float_ tex_index;
};

struct CircleVertex {
	glsl::vec3 position;
	glsl::vec3 local_position;
	glsl::vec4 color;
	glsl::float_ line_width;
	glsl::float_ fade;
};

struct ColorVertex {
	glsl::vec3 position;
	glsl::vec4 color;
};

template <typename TVertex, std::size_t V>
struct ShapeVertices {
public:
	constexpr static std::size_t count{ V };

	ShapeVertices() = default;

	// Takes in normalized color.
	ShapeVertices(
		const std::array<V2_float, count>& positions, float z_index, const V4_float& color
	) {
		PTGN_ASSERT(color.x >= 0.0f && color.y >= 0.0f && color.z >= 0.0f && color.w >= 0.0f);
		PTGN_ASSERT(color.x <= 1.0f && color.y <= 1.0f && color.z <= 1.0f && color.w <= 1.0f);
		for (std::size_t i{ 0 }; i < vertices_.size(); i++) {
			vertices_[i].position = { positions[i].x, positions[i].y, z_index };
			vertices_[i].color	  = { color.x, color.y, color.z, color.w };
		}
	}

protected:
	std::array<TVertex, count> vertices_{};
};

struct QuadVertices : public ShapeVertices<QuadVertex, 4> {
	using ShapeVertices::ShapeVertices;

	QuadVertices(
		const std::array<V2_float, count>& positions, float z_index, const V4_float& color,
		const std::array<V2_float, count>& tex_coords, float texture_index
	);
};

struct CircleVertices : public ShapeVertices<CircleVertex, 4> {
	using ShapeVertices::ShapeVertices;

	CircleVertices(
		const std::array<V2_float, count>& positions, float z_index, const V4_float& color,
		float line_width, float fade
	);
};

struct TriangleVertices : public ShapeVertices<ColorVertex, 3> {
	using ShapeVertices::ShapeVertices;
};

struct LineVertices : public ShapeVertices<ColorVertex, 2> {
	using ShapeVertices::ShapeVertices;
};

struct PointVertices : public ShapeVertices<ColorVertex, 1> {
	using ShapeVertices::ShapeVertices;
};

void OffsetVertices(std::array<V2_float, 4>& vertices, const V2_float& size, Origin draw_origin);

// Rotation angle in radians.
void RotateVertices(
	std::array<V2_float, 4>& vertices, const V2_float& position, const V2_float& size,
	float rotation_radians, const V2_float& rotation_center
);

// Rotation angle in radians.
[[nodiscard]] std::array<V2_float, 4> GetQuadVertices(
	const V2_float& position, const V2_float& size, Origin draw_origin, float rotation_radians,
	const V2_float& rotation_center
);

} // namespace ptgn::impl