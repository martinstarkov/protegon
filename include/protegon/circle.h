#pragma once

#include "vector2.h"

namespace ptgn {

template <typename T = float>
struct Circle {
	Point<T> center;
	T radius{ 0 };
	template <typename U>
	operator Circle<U>() const {
		return { static_cast<Point<U>>(center),
				 static_cast<U>(radius) };
	}
};

} // namespace ptgn