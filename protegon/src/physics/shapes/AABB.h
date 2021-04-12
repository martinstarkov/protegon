#pragma once

#include "Shape.h"

#include "math/Vector2.h"

namespace engine {

class AABB : public Shape {
public:
	AABB() = delete;
	AABB(const V2_double& size) : size{ size } {}
	virtual ShapeType GetType() const override final { return ShapeType::AABB; }
	V2_double size;
};

} // namespace engine