#pragma once

#include <array>
#include <optional>

#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/viewport.h"

namespace ptgn {

struct Camera {
	Transform transform{};
	std::optional<Viewport> raw_viewport{};
	ViewportSpace viewport_space{ ViewportSpace::Logical };
	Matrix4 view_projection{ 1.0f };

	std::array<V2_float, 4> GetWorldVertices(V2_int logical_size) const {
		Rect rect{ GetLogicalViewport(raw_viewport, viewport_space, logical_size).size };
		auto world_vertices{ rect.GetWorldVertices(transform) };
		return world_vertices;
	}
};

} // namespace ptgn