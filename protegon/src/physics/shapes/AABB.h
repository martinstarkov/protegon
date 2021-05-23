#pragma once

#include "Shape.h"

#include "math/Vector2.h"

namespace engine {

class AABB : public Shape {
public:
	AABB() = delete;
	AABB(const V2_double& size) : size{ size } {}
	virtual ShapeType GetType() const override final { return ShapeType::AABB; }
	virtual Shape* Clone() const override final { return new AABB(size); }
	V2_double size;
	friend bool operator==(const AABB& A, const AABB& B) {
		return A.size == B.size;
	}
};

} // namespace engine