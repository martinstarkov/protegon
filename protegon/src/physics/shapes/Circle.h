#pragma once

#include "math/Vector2.h"
#include "physics/shapes/Shape.h"

namespace ptgn {

class Circle : public Shape {
public:
	Circle() = default;
	~Circle() = default;
	Circle(const double radius) : radius{ radius } {}
	virtual ShapeType GetType() const override final {
		return ShapeType::CIRCLE;
	}
	virtual Shape* Clone() const override final {
		return new Circle{ radius };
	}
	virtual V2_double GetCenter(const V2_double& position) const {
		return position;
	}
	friend bool operator==(const Circle& A, const Circle& B) {
		return A.radius == B.radius;
	}
	double radius{ 0 };
};

} // namespace ptgn