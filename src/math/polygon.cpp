#include "protegon/polygon.h"

#include <vector>

#include "math/axis.h"
#include "math/utility.h"
#include "protegon/vector2.h"
#include "renderer/origin.h"
#include "utility/debug.h"

namespace ptgn {

Polygon::Polygon(const Rectangle<float>& rect, float rotation) {
	float c_a{ std::cos(rotation) };
	float s_a{ std::sin(rotation) };

	const auto rotated = [c_a, s_a](const V2_float& v) {
		return V2_float{ v.x * c_a - v.y * s_a, v.x * s_a + v.y * c_a };
	};

	vertices.resize(4);

	V2_float center{ rect.Center() };

	V2_float min{ rect.Min() - center };
	V2_float max{ rect.Max() - center };

	vertices[0] = rotated({ min.x, max.y }) + center;
	vertices[1] = rotated(max) + center;
	vertices[2] = rotated({ max.x, min.y }) + center;
	vertices[3] = rotated(min) + center;
}

Polygon::Polygon(const std::vector<V2_float>& vertices) : vertices{ vertices } {}

V2_float Polygon::GetCentroid() const {
	// Source: https://stackoverflow.com/a/63901131
	V2_float centroid;
	float signed_area{ 0.0f };
	V2_float v0{ 0.0f }; // Current verte
	V2_float v1{ 0.0f }; // Next vertex
	float a{ 0.0f };	 // Partial signed area

	std::size_t lastdex	 = vertices.size() - 1;
	const V2_float* prev = &(vertices[lastdex]);
	const V2_float* next{ nullptr };

	// For all vertices in a loop
	for (const auto& vertex : vertices) {
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

bool Polygon::Overlaps(const Polygon& polygon) const {
	const auto overlap_axis = [](const Polygon& p1, const Polygon& p2,
								 const std::vector<Axis>& axes) {
		for (Axis a : axes) {
			auto [min1, max1] = impl::GetProjectionMinMax(p1, a);
			auto [min2, max2] = impl::GetProjectionMinMax(p2, a);

			if (!impl::IntervalsOverlap(min1, max1, min2, max2)) {
				return false;
			}
		}
		return true;
	};

	if (!overlap_axis(*this, polygon, impl::GetAxes(*this, false)) ||
		!overlap_axis(polygon, *this, impl::GetAxes(polygon, false))) {
		return false;
	}
}

bool Polygon::Overlaps(const V2_float& point) const {
	const auto& v{ vertices };
	std::size_t count{ v.size() };
	bool c{ false };
	std::size_t i{ 0 };
	std::size_t j{ count - 1 };
	// Algorithm from: https://wrfranklin.org/Research/Short_Notes/pnpoly.html
	for (; i < count; j = i++) {
		bool a{ (v[i].y > point.y) != (v[j].y > point.y) };
		bool b{ point.x < (v[j].x - v[i].x) * (point.y - v[i].y) / (v[j].y - v[i].y) + v[i].x };
		if (a && b) {
			c = !c;
		}
	}
	return c;
}

bool Polygon::Contains(const Polygon& internal) const {
	for (const auto& p : internal.vertices) {
		if (!Overlaps(p)) {
			return false;
		}
	}
	return true;
}

} // namespace ptgn