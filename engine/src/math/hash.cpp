#include "math/hash.h"

#include <functional>

#include "ecs/entity.h"
#include "math/vector2.h"

namespace ptgn {

template <Arithmetic T>
std::size_t Hash(const Vector2<T>& vector) {
	return std::hash<Vector2<T>>()(vector);
}

template std::size_t Hash<int>(const Vector2<int>&);
template std::size_t Hash<float>(const Vector2<float>&);
template std::size_t Hash<double>(const Vector2<double>&);

std::size_t Hash(const Entity& entity) {
	return std::hash<Entity>()(entity);
}

} // namespace ptgn