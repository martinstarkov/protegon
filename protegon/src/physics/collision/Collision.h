#pragma once

#include "physics/Transform.h"
#include "physics/Manifold.h"
#include "physics/shapes/Shape.h"

namespace engine {

using CollisionCallback = Manifold (*)(const Transform& A, 
									   const Transform& B, 
									   Shape* const a, 
									   Shape* const b);

extern CollisionCallback StaticCollisionDispatch[static_cast<int>(ShapeType::COUNT)][static_cast<int>(ShapeType::COUNT)];

inline Manifold StaticCollisionCheck(const Transform& A, 
									 const Transform& B, 
									 Shape* const a, 
									 Shape* const b) {
	return StaticCollisionDispatch[static_cast<int>(a->GetType())][static_cast<int>(b->GetType())](A, B, a, b);
}

} // namespace engine