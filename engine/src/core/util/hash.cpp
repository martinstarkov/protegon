#include "core/util/hash.h"

#include <functional>

#include "core/math/vector2.h"
#include "core/util/concepts.h"

namespace ptgn {

template <Arithmetic T>
std::size_t Hash(Vector2<T> vector) {
	return std::hash<Vector2<T>>()(vector);
}

template std::size_t Hash<int>(Vector2<int>);
template std::size_t Hash<float>(Vector2<float>);
template std::size_t Hash<double>(Vector2<double>);

} // namespace ptgn