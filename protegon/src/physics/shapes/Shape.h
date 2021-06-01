#pragma once

#include "utils/TypeTraits.h"

namespace ptgn {

enum class ShapeType {
	CIRCLE,
	AABB,
	COUNT
};

class Shape {
public:
	virtual ~Shape() = default;
	// Returns the type of the shape.
	virtual ShapeType GetType() const = 0;
	// Clone shape (allocates heap memory).
	virtual Shape* Clone() const = 0;
	// Cast shape to a specific type.
	template <typename T,
		type_traits::is_base_of_e<Shape, T> = true>
	T& CastTo() {
		return *static_cast<T*>(this);
	}
};

} // namespace ptgn