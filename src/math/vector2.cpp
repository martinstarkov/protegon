#include "math/vector2.h"

namespace ptgn {

template <typename T>
bool Vector2<T>::IsZero() const noexcept {
	return NearlyEqual(x, T{ 0 }) && NearlyEqual(y, T{ 0 });
}

template struct Vector2<int>;
template struct Vector2<float>;
template struct Vector2<double>;

} // namespace ptgn
