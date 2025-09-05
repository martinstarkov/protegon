#include "math/geometry/line.h"

#include <array>
#include <utility>

#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "math/geometry.h"
#include "math/geometry/rect.h"
#include "math/vector2.h"
#include "renderer/render_data.h"

namespace ptgn {

Line::Line(const V2_float& start, const V2_float& end) : start{ start }, end{ end } {}

void Line::Draw(impl::RenderData& ctx, const Entity& entity) {
	impl::DrawLine(ctx, entity);
}

std::array<V2_float, 4> Line::GetWorldQuadVertices(
	const Transform& transform, float line_width, float x_padding, V2_float* out_size
) const {
	auto dir{ end - start };

	//  TODO: Fix right and top side of line being 1 pixel thicker than left and bottom.
	auto local_center{ start + dir * 0.5f };

	V2_float center{ ApplyTransform(local_center, transform) };

	float rotation{ dir.Angle() };
	V2_float size{ dir.Magnitude() + x_padding, line_width };
	if (out_size) {
		*out_size = size;
	}
	Rect rect{ size };
	return rect.GetWorldVertices(Transform{ center, rotation });
}

std::array<V2_float, 2> Line::GetWorldVertices(const Transform& transform) const {
	auto local_vertices{ GetLocalVertices() };
	return ApplyTransform(local_vertices, transform);
}

std::array<V2_float, 2> Line::GetLocalVertices() const {
	return { start, end };
}

} // namespace ptgn