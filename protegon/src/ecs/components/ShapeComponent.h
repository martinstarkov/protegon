#pragma once

#include <memory>

#include "physics/shapes/Shape.h"
#include "physics/shapes/AABB.h"
#include "physics/shapes/Circle.h"
#include "utils/TypeTraits.h"

namespace engine {

struct ShapeComponent {
	ShapeComponent() = delete;
	template <typename T, 
		type_traits::is_base_of<Shape, T> = true>
	ShapeComponent(const T& shape) : shape{ new T(shape) } {}
	Shape* shape;
};

} // namespace engine