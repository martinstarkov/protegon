#pragma once

#include <memory> // std::unique_ptr, std::make_unique

#include "physics/shapes/Shape.h"

namespace ptgn {

namespace physics {

class AABB : public internal::physics::Shape {
public:
	AABB() = default;
	~AABB() = default;
	
	AABB(const V2_double& size) : size{ size } {}
	
	virtual internal::physics::ShapeType GetType() const override final {
		return internal::physics::ShapeType::AABB;
	}
	
	virtual std::unique_ptr<Shape> Clone() const override final {
		return std::make_unique<AABB>(size);
	}
	
	virtual V2_double GetCenter(const V2_double& position) const override final {
		return position + size / 2.0;
	}
	
	// Returns vector with { width, height }.
	virtual V2_double GetSize() const override final {
		return size;
	}

	friend bool operator==(const AABB& A, const AABB& B) {
		return A.size == B.size;
	}

	V2_double size;
};

} // namespace physics

} // namespace ptgn