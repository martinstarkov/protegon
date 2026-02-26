#include "core/util/hash.h"

#include <functional>

#include "core/math/vector2.h"

namespace ptgn {

template <Arithmetic T>
std::size_t Hash(Vector2<T> vector) {
	return std::hash<Vector2<T>>()(vector);
}

template std::size_t Hash<int>(V2_int);
template std::size_t Hash<float>(V2_float);
template std::size_t Hash<double>(V2_double);

} // namespace ptgn