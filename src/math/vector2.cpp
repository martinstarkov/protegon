#include "math/vector2.h"

#include "math/collision/overlap.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "serialization/fwd.h"
#include "serialization/json.h"
#include "utility/assert.h"

namespace ptgn {

template <typename T>
void to_json(json& j, const Vector2<T>& v) {
	j = json::array({ v.x, v.y });
}

template <typename T>
void from_json(const json& j, Vector2<T>& v) {
	PTGN_ASSERT(j.is_array(), "Cannot create Vector2 from json object which is not an array");
	PTGN_ASSERT(j.size() <= 2, "Cannot create Vector2 from json array with more than 2 elements");
	if (j.size() == 2) {
		j[0].get_to(v.x);
		j[1].get_to(v.y);
	} else if (j.size() == 1) {
		j[0].get_to(v.x);
		j[0].get_to(v.y);
	} else if (j.empty()) {
		v = {};
	}
}

template <typename T>
bool Vector2<T>::IsZero() const noexcept {
	return NearlyEqual(x, T{ 0 }) && NearlyEqual(y, T{ 0 });
}

template <typename T>
bool Vector2<T>::Overlaps(const Line& line) const {
	return OverlapPointLine(V2_float{ *this }, line.GetStart(), line.GetEnd());
}

template <typename T>
bool Vector2<T>::Overlaps(const Circle& circle) const {
	return OverlapPointCircle(V2_float{ *this }, circle.GetCenter(), circle.GetRadius());
}

template <typename T>
bool Vector2<T>::Overlaps(const Rect& rect) const {
	auto [rect_min, rect_max] = rect.GetExtents();
	return OverlapPointRect(
		V2_float{ *this }, rect_min, rect_max, rect.GetRotation(), rect.GetRotationCenter()
	);
}

template <typename T>
bool Vector2<T>::Overlaps(const Triangle& triangle) const {
	auto [a, b, c] = triangle.GetVertices();
	return OverlapPointTriangle(V2_float{ *this }, a, b, c);
}

template <typename T>
bool Vector2<T>::Overlaps(const Capsule& capsule) const {
	return OverlapPointCapsule(
		V2_float{ *this }, capsule.GetStart(), capsule.GetEnd(), capsule.GetRadius()
	);
}

template <typename T>
bool Vector2<T>::Overlaps(const Polygon& polygon) const {
	return OverlapPointPolygon(V2_float{ *this }, polygon.GetVertices());
}

template struct Vector2<int>;
template struct Vector2<float>;
template struct Vector2<double>;

template void from_json(const json& j, Vector2<int>& v);
template void from_json(const json& j, Vector2<float>& v);
template void from_json(const json& j, Vector2<double>& v);

template void to_json(json& j, const Vector2<int>& v);
template void to_json(json& j, const Vector2<float>& v);
template void to_json(json& j, const Vector2<double>& v);

} // namespace ptgn
