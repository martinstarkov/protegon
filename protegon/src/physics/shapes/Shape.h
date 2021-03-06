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
	
	// Returns the center position of the shape given its position.
	virtual V2_double GetCenter(const V2_double& position) const = 0;

	/*
	* @return AABB: { width, height }, Circle: { diameter, diameter }
	*/
	virtual V2_double GetSize() const = 0;
	
	// Cast shape to a specific type.
	template <typename T,
		type_traits::is_base_of_e<Shape, T> = true>
	T& CastTo() {
		return *static_cast<T*>(this);
	}
};

} // namespace ptgn