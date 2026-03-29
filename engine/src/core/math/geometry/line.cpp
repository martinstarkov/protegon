#include "core/math/geometry/line.h"

#include <array>

#include "core/assert.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"

namespace ptgn {

Line::Line(V2_float start, V2_float end) : start{ start }, end{ end } {}

std::array<V2_float, 4> Line::GetWorldQuadVertices(
	Transform transform, float line_width, V2_float* out_size
) const {
	PTGN_ASSERT(line_width >= 1.0f);

	auto dir{ end - start };

	auto local_center{ start + dir * 0.5f };

	V2_float center{ transform.Apply(local_center) };

	float rotation{ dir.Angle() };

	Rect rect{ V2_float{ dir.Magnitude() + line_width, line_width } };

	if (out_size) {
		*out_size = rect.GetSize(transform);
	}

	Transform rect_transform{ center, rotation, transform.GetScale() };

	return rect.GetWorldVertices(rect_transform);
}

std::array<V2_float, 2> Line::GetWorldVertices(Transform transform) const {
	auto local_vertices{ GetLocalVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 2> Line::GetLocalVertices() const {
	return { start, end };
}

} // namespace ptgn