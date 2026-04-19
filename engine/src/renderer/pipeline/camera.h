#pragma once

#include <array>

#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/viewport.h"

namespace ptgn {

struct Camera {
	Transform transform;
	Viewport viewport;
	Matrix4 view_projection;

	std::array<V2_float, 4> GetWorldVertices() const {
		Rect rect{ viewport.size };
		auto world_vertices{ rect.GetWorldVertices(transform) };
		return world_vertices;
	}
};

} // namespace ptgn