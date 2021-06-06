#pragma once

#include "math/Vector2.h"
#include "physics/shapes/Shape.h"

namespace ptgn {

class AABB : public Shape {
public:
	AABB() = default;
	~AABB() = default;
	AABB(const V2_double& size) : size{ size } {}
	virtual ShapeType GetType() const override final {
		return ShapeType::AABB;
	}
	virtual Shape* Clone() const override final {
		return new AABB{ size };
	}
	virtual V2_double GetCenter(const V2_double& position) const {
		return position + size / 2.0;
	}
	friend bool operator==(const AABB& A, const AABB& B) {
		return A.size == B.size;
	}
	V2_double size;
};

} // namespace ptgn