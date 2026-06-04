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
	ViewportSpace viewport_space{ ViewportSpace::Game };
	Matrix4 view_projection;

	std::array<V2_float, 4> GetWorldVertices() const {
		Rect rect{ viewport.size };
		auto world_vertices{ rect.GetWorldVertices(transform) };
		return world_vertices;
	}

	friend bool operator==(const Camera& lhs, const Camera& rhs) {
		// View projection omitted because it is derived from transform and viewport, so if those
		// are equal, the view projection must be equal as well.
		return lhs.transform == rhs.transform && lhs.viewport == rhs.viewport;
	}
};

} // namespace ptgn