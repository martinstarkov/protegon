//#pragma once
//
//#include <ostream> // std::ostream
//
//#include "math/Vector2.h"
//
//struct AABB {
//	V2_double position;
//	V2_double size;
//	AABB() = default;
//	AABB(const V2_double& position, const V2_double& size) : position{ position }, size{ size } {}
//	AABB(int x, int y, int w, int h) : position{ x, y }, size{ w, h } {}
//	AABB ExpandedBy(const AABB& other) const {
//		return { position - other.size / 2.0, size + other.size };
//	}
//	V2_double Center() {
//		return position + size / 2.0;
//	}
//	V2_double Center() const {
//		return position + size / 2.0;
//	}
//	// Returns an AABB which encompasses the initial position and the future position of a dynamic AABB.
//	AABB GetBroadphaseBox(const V2_double& velocity) const {
//		AABB broadphase_box;
//		broadphase_box.position.x = velocity.x > 0.0 ? position.x : position.x + velocity.x;
//		broadphase_box.position.y = velocity.y > 0.0 ? position.y : position.y + velocity.y;
//		broadphase_box.size.x = velocity.x > 0.0 ? velocity.x + size.x : size.x - velocity.x;
//		broadphase_box.size.y = velocity.y > 0.0 ? velocity.y + size.y : size.y - velocity.y;
//		return broadphase_box;
//	}
//	friend std::ostream& operator<<(std::ostream& os, const AABB& obj) {
//		os << '[' << obj.position << ',' << obj.size << ']';
//		return os;
//	}
//	friend bool operator==(const AABB& A, const AABB& B) {
//		return A.position == B.position && A.size == B.size;
//	}
//};