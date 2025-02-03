#include "math/vector2.h"

#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "serialization/json.h"
#include "utility/debug.h"

namespace ptgn {

template <typename T>
Vector2<T>::Vector2(const json& j) {
	PTGN_ASSERT(j.is_array(), "Cannot create Vector2 from json object which is not an array");
	PTGN_ASSERT(
		j.size() == 2,
		"Cannot create Vector2 from json array object which is not exactly 2 elements"
	);
	j[0].get_to(x);
	j[1].get_to(y);
}

template <typename T>
bool Vector2<T>::IsZero() const {
	return NearlyEqual(x, T{ 0 }) && NearlyEqual(y, T{ 0 });
}

template <typename T>
bool Vector2<T>::Overlaps(const Line& line) const {
	return line.Overlaps(V2_float{ *this });
}

template <typename T>
bool Vector2<T>::Overlaps(const Circle& circle) const {
	return circle.Overlaps(V2_float{ *this });
}

template <typename T>
bool Vector2<T>::Overlaps(const Rect& rect) const {
	return rect.Overlaps(V2_float{ *this });
}

template <typename T>
bool Vector2<T>::Overlaps(const Capsule& capsule) const {
	return capsule.Overlaps(V2_float{ *this });
}

template struct Vector2<int>;
template struct Vector2<float>;
template struct Vector2<double>;

} // namespace ptgn
