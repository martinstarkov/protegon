#pragma once

#include <utility> // std::exchange
#include <cassert> // assert
#include <memory> // std::unique_ptr, std::make_unique

#include "physics/shapes/Shape.h"
#include "physics/shapes/AABB.h"
#include "physics/shapes/Circle.h"
#include "utils/TypeTraits.h"

namespace ptgn {

namespace component {

struct Shape {
	Shape() = delete;
	template <typename T, type_traits::is_base_of_e<internal::physics::Shape, T> = true>
	Shape(const T& shape, const V2_double& offset = {}) :
		instance{ std::make_unique<T>(shape) },
		offset{ offset } {
	}

	Shape& operator=(const Shape& copy) noexcept {
		instance = std::move(copy.instance->Clone());
		offset = copy.offset;
		return *this;
	}
	Shape(const Shape& copy) noexcept {
		*this = copy;
	}

	Shape(Shape&& move) = default;
	Shape& operator=(Shape&& move) = default;

	V2_double offset;
	std::unique_ptr<internal::physics::Shape> instance{ nullptr };
};

struct OriginalShape : public Shape {};

} // namespace component

} // namespace ptgn