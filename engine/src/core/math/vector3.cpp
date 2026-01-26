#include "math/vector3.h"

#include "serialization/json/json.h"

namespace ptgn {

template <Arithmetic T>
Vector3<T>::Vector3(const json& j) {
	j.get_to(*this);
}

template <Arithmetic T>
void to_json(json& j, const Vector3<T>& vector) {
	j = json::array({ vector.x, vector.y, vector.z });
}

template <Arithmetic T>
void from_json(const json& j, Vector3<T>& vector) {
	PTGN_ASSERT(j.is_array(), "Deserializing a Vector3 from json requires an array");
	PTGN_ASSERT(
		j.size() == 3, "Deserializing a Vector3 from json requires an array with two elements"
	);
	vector.x = j[0];
	vector.y = j[1];
	vector.z = j[2];
}

template struct Vector3<int>;
template struct Vector3<float>;
template struct Vector3<double>;

template void to_json<int>(json&, const Vector3<int>&);
template void from_json<int>(const json&, Vector3<int>&);
template void to_json<float>(json&, const Vector3<float>&);
template void from_json<float>(const json&, Vector3<float>&);
template void to_json<double>(json&, const Vector3<double>&);
template void from_json<double>(const json&, Vector3<double>&);

} // namespace ptgn
