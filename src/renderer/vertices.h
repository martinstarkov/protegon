#pragma once

#include <array>

#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/buffer_layout.h"
#include "renderer/gl_helper.h"
#include "utility/debug.h"

namespace ptgn {

struct ColorVertex {
	glsl::vec3 position;
	glsl::vec4 color;
};

constexpr inline const BufferLayout<glsl::vec3, glsl::vec4> color_vertex_layout;

struct QuadVertex {
	glsl::vec3 position;
	glsl::vec4 color;
	glsl::vec2 tex_coord;
	glsl::float_ tex_index;
};

constexpr inline const BufferLayout<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_>
	quad_vertex_layout;

struct CircleVertex {
	glsl::vec3 position;
	glsl::vec4 color;
	glsl::vec3 local_position;
	glsl::float_ line_width;
	glsl::float_ fade;
};

constexpr inline const BufferLayout<glsl::vec3, glsl::vec4, glsl::vec3, glsl::float_, glsl::float_>
	circle_vertex_layout;

namespace impl {

template <typename TVertex, std::size_t V, PrimitiveMode M, typename TLayout>
struct ShapeVertices {
public:
	constexpr static std::size_t count{ V };
	constexpr static PrimitiveMode mode{ M };
	constexpr static TLayout layout{};

	ShapeVertices() = default;

	// Takes in normalized color.
	ShapeVertices(
		const std::array<V2_float, count>& vertices, float z_index, const V4_float& color
	) {
		PTGN_ASSERT(color.x >= 0.0f && color.y >= 0.0f && color.z >= 0.0f && color.w >= 0.0f);
		PTGN_ASSERT(color.x <= 1.0f && color.y <= 1.0f && color.z <= 1.0f && color.w <= 1.0f);
		for (std::size_t i{ 0 }; i < vertices_.size(); i++) {
			vertices_[i].position = { vertices[i].x, vertices[i].y, z_index };
			vertices_[i].color	  = { color.x, color.y, color.z, color.w };
		}
	}

	[[nodiscard]] const std::array<TVertex, count>& Get() const {
		return vertices_;
	}

	[[nodiscard]] static constexpr TLayout GetLayout() {
		return TLayout{};
	}

protected:
	std::array<TVertex, count> vertices_{};
};

struct QuadVertices :
	public ShapeVertices<QuadVertex, 4, PrimitiveMode::Triangles, decltype(quad_vertex_layout)> {
	using ShapeVertices::ShapeVertices;

	QuadVertices(
		const std::array<V2_float, count>& vertices, float z_index, const V4_float& color,
		const std::array<V2_float, count>& tex_coords, float texture_index
	);
};

struct TextureVertices :
	public ShapeVertices<
		TextureVertex, 4, PrimitiveMode::Triangles, decltype(texture_vertex_layout)> {
	using ShapeVertices::ShapeVertices;

	TextureVertices(
		const std::array<V2_float, count>& vertices, const std::array<V2_float, count>& tex_coords,
		float z_index, const V4_float& color
	);
};

struct CircleVertices :
	public ShapeVertices<
		CircleVertex, 4, PrimitiveMode::Triangles, decltype(circle_vertex_layout)> {
	using ShapeVertices::ShapeVertices;

	CircleVertices(
		const std::array<V2_float, count>& vertices, float z_index, const V4_float& color,
		float line_width, float fade
	);
};

struct TriangleVertices :
	public ShapeVertices<ColorVertex, 3, PrimitiveMode::Triangles, decltype(color_vertex_layout)> {
	using ShapeVertices::ShapeVertices;
};

struct LineVertices :
	public ShapeVertices<ColorVertex, 2, PrimitiveMode::Lines, decltype(color_vertex_layout)> {
	using ShapeVertices::ShapeVertices;
};

struct PointVertices :
	public ShapeVertices<ColorVertex, 1, PrimitiveMode::Points, decltype(color_vertex_layout)> {
	using ShapeVertices::ShapeVertices;
};

struct ColorQuadVertices :
	public ShapeVertices<ColorVertex, 4, PrimitiveMode::Triangles, decltype(color_vertex_layout)> {
	using ShapeVertices::ShapeVertices;
};

} // namespace impl

} // namespace ptgn