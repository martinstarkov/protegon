
#include "math/geometry/triangle.h"

#include <array>

#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "renderer/render_data.h"

namespace ptgn {

Triangle::Triangle(const V2_float& a, const V2_float& b, const V2_float& c) :
	a{ a }, b{ b }, c{ c } {}

Triangle::Triangle(const std::array<V2_float, 3>& vertices) :
	a{ vertices[0] }, b{ vertices[1] }, c{ vertices[2] } {}

void Triangle::Draw(impl::RenderData& ctx, const Entity& entity) {
	impl::DrawTriangle(ctx, entity);
}

std::array<V2_float, 3> Triangle::GetWorldVertices(const Transform& transform) const {
	auto local_vertices{ GetLocalVertices() };
	return transform.Apply(local_vertices);
}

std::array<V2_float, 3> Triangle::GetLocalVertices() const {
	return { a, b, c };
}

} // namespace ptgn