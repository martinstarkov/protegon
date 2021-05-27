#pragma once

#include "physics/Transform.h"
#include "physics/RigidBody.h"
#include "physics/shapes/Shape.h"
#include "physics/shapes/AABB.h"
#include "physics/shapes/Circle.h"
#include "renderer/Color.h"
#include "utils/Countdown.h"
#include "utils/TypeTraits.h"

namespace engine {

using ParticleLifetime = Countdown;

template <typename TShape,
	type_traits::is_base_of_e<Shape, TShape> = true>
struct ParticleAppearance {
	TShape shape_begin;
	TShape shape_end;
	Color color_begin;
	Color color_end;
};

struct ParticleProperties {
	Transform transform;
	RigidBody body;
};

} // namespace engine