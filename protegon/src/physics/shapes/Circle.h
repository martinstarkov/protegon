#pragma once

#include "Shape.h"

#include "math/Vector2.h"

namespace engine {

class Circle : public Shape {
public:
	Circle() = delete;
	Circle(const double radius) : radius{ radius } {}
	virtual ShapeType GetType() const override final { return ShapeType::CIRCLE; }
	double radius{ 0 };
};

} // namespace engine