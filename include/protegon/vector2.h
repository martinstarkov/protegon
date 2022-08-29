#pragma once

namespace ptgn {

template <typename T>
struct Vector2 {
	T x{};
	T y{};
};

using V2_int = Vector2<int>;
using V2_float = Vector2<float>;

} // namespace ptgn