#pragma once

#include "physics/Transform.h"
#include "physics/RigidBody.h"
#include "physics/shapes/Shape.h"
#include "physics/shapes/AABB.h"
#include "physics/shapes/Circle.h"
#include "renderer/Color.h"
#include "utils/Countdown.h"

namespace engine {

struct Particle {
	Countdown lifetime;
	Shape* begin_shape{ nullptr };
	Shape* end_shape{ nullptr };
	Color begin_color;
	Color end_color;
};

struct ParticleProperties {
	Transform transform;
	RigidBody body;
};

} // namespace engine