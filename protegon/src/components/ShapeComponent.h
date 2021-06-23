#pragma once

#include <utility> // std::exchange
#include <cassert> // assert

#include "physics/shapes/Shape.h"
#include "physics/shapes/AABB.h"
#include "physics/shapes/Circle.h"
#include "utils/TypeTraits.h"

namespace ptgn {

struct ShapeComponent {
	ShapeComponent() = delete;

	~ShapeComponent() {
		delete shape;
	}

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

	Shape* shape{ nullptr };
};

} // namespace ptgn