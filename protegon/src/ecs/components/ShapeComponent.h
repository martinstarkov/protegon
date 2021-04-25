#pragma once

#include "physics/shapes/Shape.h"
#include "physics/shapes/AABB.h"
#include "physics/shapes/Circle.h"
#include "utils/TypeTraits.h"

// TODO: Consider switching to smart pointer here.

namespace engine {

struct ShapeComponent {
	ShapeComponent() = delete;
	template <typename T,
	engine::type_traits::is_base_of<Shape, T> = true>
	ShapeComponent(const T& shape) : shape{ new T(shape) } {}
	V2_double GetSize() const {
		switch (shape->GetType()) {
			case ShapeType::AABB: {
				return shape->CastTo<AABB>().size;
			}
			case ShapeType::CIRCLE: {
				auto diameter{ 2 * shape->CastTo<Circle>().radius };
				return { diameter, diameter };
			}
			default: {
				return {};
			}
		}
		return {};
	}
	Shape* shape{ nullptr };
};

} // namespace engine