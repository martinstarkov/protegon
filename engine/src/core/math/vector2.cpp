#include "core/math/vector2.h"

#include <nlohmann/json.hpp>

#include "core/assert.h"
#include "core/math/tolerance.h"
#include "core/util/concepts.h"
#include "serialization/json/fwd.h"

namespace ptgn {

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

bool StrictlyLess(V2_float a, V2_float b, float epsilon) {
	return StrictlyLess(a.x, b.x, epsilon) && StrictlyLess(a.y, b.y, epsilon);
}

template void to_json<int>(json&, const Vector2<int>&);
template void from_json<int>(const json&, Vector2<int>&);
template void to_json<float>(json&, const Vector2<float>&);
template void from_json<float>(const json&, Vector2<float>&);

} // namespace ptgn
