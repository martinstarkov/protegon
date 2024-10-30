#include "renderer/vertices.h"

#include <array>

#include "math/vector2.h"
#include "math/vector4.h"

namespace ptgn::impl {

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

TextureVertices::TextureVertices(
	const std::array<V2_float, count>& positions, const std::array<V2_float, count>& tex_coords,
	float z_index
) {
	for (std::size_t i{ 0 }; i < vertices_.size(); i++) {
		vertices_[i].position  = { positions[i].x, positions[i].y, z_index };
		vertices_[i].tex_coord = { tex_coords[i].x, tex_coords[i].y };
	}
}

} // namespace ptgn::impl