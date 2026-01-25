#include "core/util/hash.h"

#include <functional>

#include "ecs/entity.h"
#include "math/vector2.h"

namespace ptgn {

template <Arithmetic T>
std::size_t Hash(Vector2<T> vector) {
	return std::hash<Vector2<T>>()(vector);
}

template std::size_t Hash<int>(const V2_int&);
template std::size_t Hash<float>(const V2_float&);
template std::size_t Hash<double>(const V2_double&);

std::size_t Hash(const Entity& entity) {
	return std::hash<Entity>()(entity);
}

} // namespace ptgn