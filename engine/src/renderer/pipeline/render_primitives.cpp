#include "renderer/pipeline/render_primitives.h"

#include <array>

#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/vertex.h"

namespace ptgn::impl {

ColorTriangle CreateColorTriangle(
	const std::array<V2_float, 3>& vertices, Depth depth, V4_float color_n, int entity
) {
	return { ColorVertex{ vertices[0], depth, color_n, entity },
			 ColorVertex{ vertices[1], depth, color_n, entity },
			 ColorVertex{ vertices[2], depth, color_n, entity } };
}

ColorQuad CreateColorQuad(
	const std::array<V2_float, 4>& vertices, Depth depth, V4_float color_n, int entity
) {
	return { ColorVertex{ vertices[0], depth, color_n, entity },
			 ColorVertex{ vertices[1], depth, color_n, entity },
			 ColorVertex{ vertices[2], depth, color_n, entity },
			 ColorVertex{ vertices[3], depth, color_n, entity } };
}

ShapeQuad CreateShapeQuad(
	const std::array<V2_float, 4>& vertices, Depth depth, V4_float color_n,
	const std::array<V2_float, 4>& local_coords, const std::array<float, 4>& data, int entity
) {
	return { ShapeVertex{ vertices[0], depth, color_n, local_coords[0], data, entity },
			 ShapeVertex{ vertices[1], depth, color_n, local_coords[1], data, entity },
			 ShapeVertex{ vertices[2], depth, color_n, local_coords[2], data, entity },
			 ShapeVertex{ vertices[3], depth, color_n, local_coords[3], data, entity } };
}

TextureQuad CreateTextureQuad(
	const std::array<V2_float, 4>& vertices, Depth depth, V4_float color_n,
	const std::array<V2_float, 4>& tex_coords, int entity
) {
	constexpr auto texture_index{ 0.0f };

	return {
		TextureVertex{ vertices[0], depth, color_n, tex_coords[0], texture_index, entity },
		TextureVertex{ vertices[1], depth, color_n, tex_coords[1], texture_index, entity },
		TextureVertex{ vertices[2], depth, color_n, tex_coords[2], texture_index, entity },
		TextureVertex{ vertices[3], depth, color_n, tex_coords[3], texture_index, entity },
	};
}

} // namespace ptgn::impl