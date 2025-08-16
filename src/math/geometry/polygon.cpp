#include "math/geometry/polygon.h"

#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/transform.h"
#include "core/entity.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "renderer/render_data.h"

namespace ptgn {

void Polygon::Draw(impl::RenderData& ctx, const Entity& entity) {
	impl::DrawPolygon(ctx, entity);
}

V2_float Polygon::GetCenter() const {
	PTGN_ASSERT(vertices.size() >= 3);
	// Source: https://stackoverflow.com/a/63901131
	V2_float centroid;
	float signed_area{ 0.0f };
	V2_float v0{ 0.0f }; // Current verte
	V2_float v1{ 0.0f }; // Next vertex
	float a{ 0.0f };	 // Partial signed area

	std::size_t lastdex{ vertices.size() - 1 };
	const V2_float* prev = &(vertices[lastdex]);
	const V2_float* next{ nullptr };

	// For all vertices in a loop
	for (std::size_t i{ 0 }; i < vertices.size(); i++) {
		const auto& vertex{ vertices[i] };
		next		 = &vertex;
		v0			 = *prev;
		v1			 = *next;
		a			 = v0.Cross(v1);
		signed_area += a;
		centroid	+= (v0 + v1) * a;
		prev		 = next;
	}

	signed_area *= 0.5f;
	centroid	/= 6.0f * signed_area;

	return centroid;
}

std::vector<V2_float> Polygon::GetWorldVertices(const Transform& transform) const {
	return ToWorldPoint(vertices, transform);
}

std::vector<V2_float> Polygon::GetLocalVertices() const {
	return vertices;
}

} // namespace ptgn