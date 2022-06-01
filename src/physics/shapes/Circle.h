#pragma once

#include <memory> // std::unique_ptr, std::make_unique

#include "physics/shapes/Shape.h"

namespace ptgn {

namespace physics {

class Circle : public internal::physics::Shape {
public:
	Circle() = default;
	~Circle() = default;
	
	Circle(const double radius) : radius{ radius } {}
	
	virtual internal::physics::ShapeType GetType() const override final {
		return internal::physics::ShapeType::CIRCLE;
	}
	
	virtual std::unique_ptr<Shape> Clone() const override final {
		return std::make_unique<Circle>(radius);
	}
	
	virtual V2_double GetCenter(const V2_double& position) const override final {
		return position;
	}

	// Returns vector with { diameter, diameter }.
	virtual V2_double GetSize() const override final {
		auto diameter{ 2.0 * radius };
		return { diameter, diameter };
	}
	
	bool operator==(const Circle& B) const {
		return radius == B.radius;
	}

	bool operator!=(const Circle& B) const {
		return !operator==(B);
	}
	
	double radius{ 0 };
};

} // namespace physics

} // namespace ptgn