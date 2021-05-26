#pragma once

#include <utility> // std::exchange
#include <cassert> // assert

#include "physics/shapes/Shape.h"
#include "physics/shapes/AABB.h"
#include "physics/shapes/Circle.h"
#include "utils/TypeTraits.h"

namespace engine {

struct ShapeComponent {
	ShapeComponent() = delete;

	ShapeComponent(Shape* shape) noexcept {
		assert(shape != nullptr &&
			   "Cannot clone ShapeComponent from null shape");
		this->shape = shape->Clone();
	}

	template <typename T,
		type_traits::is_base_of_e<Shape, T> = true>
	ShapeComponent(const T& shape) : shape{ new T(shape) } {}

	ShapeComponent(const ShapeComponent& copy) noexcept {
		assert(copy.shape != nullptr &&
			   "Cannot copy ShapeComponent from null shape");
		delete shape;
		shape = copy.shape->Clone();
	}

	ShapeComponent(ShapeComponent&& move) noexcept {
		assert(move.shape != nullptr &&
			   "Cannot move ShapeComponent from null shape");
		delete shape;
		shape = std::exchange(move.shape, nullptr);
	}
	
	~ShapeComponent() {
		delete shape;
	}

	/*
	* @return AABB: { width, height }, Circle: { diameter, diameter }
	*/
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

	Shape* operator->() {
		return shape;
	}

	const Shape* operator->() const {
		return shape;
	}

	Shape* shape{ nullptr };
};

} // namespace engine