#include "math/vector4.h"

#include "serialization/json.h"

namespace ptgn {

template <Arithmetic T>
Vector4<T>::Vector4(const json& j) {
	j.get_to(*this);
}

template <Arithmetic T>
void to_json(json& j, const Vector4<T>& vector) {
	j = json::array({ vector.x, vector.y, vector.w, vector.z });
}

template <Arithmetic T>
void from_json(const json& j, Vector4<T>& vector) {
	PTGN_ASSERT(j.is_array(), "Deserializing a Vector4 from json requires an array");
	PTGN_ASSERT(
		j.size() == 4, "Deserializing a Vector4 from json requires an array with four elements"
	);
	vector.x = j[0];
	vector.y = j[1];
	vector.w = j[2];
	vector.z = j[3];
}

template struct Vector4<int>;
template struct Vector4<float>;
template struct Vector4<double>;

template void to_json<int>(json&, const Vector4<int>&);
template void from_json<int>(const json&, Vector4<int>&);
template void to_json<float>(json&, const Vector4<float>&);
template void from_json<float>(const json&, Vector4<float>&);
template void to_json<double>(json&, const Vector4<double>&);
template void from_json<double>(const json&, Vector4<double>&);

} // namespace ptgn