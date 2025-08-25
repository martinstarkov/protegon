#include "math/vector2.h"

#include "serialization/json.h"

namespace ptgn {

template <Arithmetic T>
Vector2<T>::Vector2(const json& j) {
	j.get_to(*this);
}

template <Arithmetic T>
void to_json(json& j, const Vector2<T>& vector) {
	j = json::array({ vector.x, vector.y });
}

template <Arithmetic T>
void from_json(const json& j, Vector2<T>& vector) {
	PTGN_ASSERT(j.is_array(), "Deserializing a Vector2 from json requires an array");
	PTGN_ASSERT(
		j.size() == 2, "Deserializing a Vector2 from json requires an array with two elements"
	);
	vector.x = j[0];
	vector.y = j[1];
}

template struct Vector2<int>;
template struct Vector2<float>;
template struct Vector2<double>;

template void to_json<int>(json&, const Vector2<int>&);
template void from_json<int>(const json&, Vector2<int>&);
template void to_json<float>(json&, const Vector2<float>&);
template void from_json<float>(const json&, Vector2<float>&);
template void to_json<double>(json&, const Vector2<double>&);
template void from_json<double>(const json&, Vector2<double>&);

} // namespace ptgn
