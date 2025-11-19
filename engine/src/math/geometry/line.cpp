#include "math/geometry/line.h"

#include <array>

#include "ecs/components/transform.h"
#include "math/geometry/rect.h"
#include "math/vector2.h"

namespace ptgn {

Line::Line(const V2_float& start, const V2_float& end) : start{ start }, end{ end } {}

std::array<V2_float, 4> Line::GetWorldQuadVertices(
	const Transform& transform, float line_width, V2_float* out_size
) const {
	auto dir{ end - start };

	auto local_center{ start + dir * 0.5f };

	V2_float center{ transform.Apply(local_center) };

	float rotation{ dir.Angle() };
	Rect rect{ V2_float{ dir.Magnitude() + line_width, line_width } };
	if (out_size) {
		*out_size = rect.GetSize(transform);
	}
	return rect.GetWorldVertices(Transform{ center, rotation, transform.GetScale() });
}

std::array<V2_float, 2> Line::GetWorldVertices(const Transform& transform) const {
	auto local_vertices{ GetLocalVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 2> Line::GetLocalVertices() const {
	return { start, end };
}

} // namespace ptgn