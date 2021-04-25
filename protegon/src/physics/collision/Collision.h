#pragma once

#include "physics/Manifold.h"
#include "physics/shapes/Shape.h"

namespace engine {

using CollisionCallback = Manifold (*)(const Transform& A, const Transform& B, Shape* const A_s, Shape* const B_s);

extern CollisionCallback StaticCollisionDispatch[static_cast<int>(ShapeType::COUNT)][static_cast<int>(ShapeType::COUNT)];

inline Manifold StaticCollisionCheck(const Transform& A, const Transform& B, Shape* const A_s, Shape* const B_s) {
	return StaticCollisionDispatch[static_cast<int>(A_s->GetType())][static_cast<int>(B_s->GetType())](A, B, A_s, B_s);
}

} // namespace engine