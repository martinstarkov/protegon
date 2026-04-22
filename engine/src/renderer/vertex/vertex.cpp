#include "renderer/vertex/vertex.h"

#include <array>
#include <utility>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/graphics/flip.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"

namespace ptgn::impl {

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

	using enum Flip;

	if (flip_x && flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Both);
	} else if (flip_x) {
		impl::FlipTextureCoordinates(tex_coords, Horizontal);
	} else if (flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Vertical);
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
		using enum Flip;
		case None:		 break;
		case Vertical:	 flip_y(); break;
		case Horizontal: flip_x(); break;
		case Both:
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

ColorVertex::ColorVertex(V2_float position, float depth, V4_float color, int entity_id) :
	position{ position.x, position.y, depth },
	color{ color[0], color[1], color[2], color[3] },
	entity_id{ entity_id } {}

ShapeVertex::ShapeVertex(
	V2_float position, float depth, V4_float color, V2_float local_coord,
	const std::array<float, 4>& shape_data, int entity_id
) :
	position{ position.x, position.y, depth },
	color{ color[0], color[1], color[2], color[3] },
	local_coord{ local_coord.x, local_coord.y },
	shape_data{ shape_data },
	entity_id{ entity_id } {}

TextureVertex::TextureVertex(
	V2_float position, float depth, V4_float color, V2_float tex_coord, float tex_index,
	int entity_id
) :
	position{ position.x, position.y, depth },
	color{ color[0], color[1], color[2], color[3] },
	tex_coord{ tex_coord.x, tex_coord.y },
	tex_index{ tex_index },
	entity_id{ entity_id } {}

} // namespace ptgn::impl