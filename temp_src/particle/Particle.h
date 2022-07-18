#pragma once

#include "physics/Transform.h"
#include "physics/RigidBody.h"
#include "physics/shapes/Shape.h"
#include "physics/shapes/AABB.h"
#include "physics/shapes/Circle.h"
#include "renderer/Color.h"
#include "utility/Countdown.h"
#include "utility/TypeTraits.h"

namespace ptgn {

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

} // namespace ptgn