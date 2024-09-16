#pragma once

#include "ecs/ecs.h"
#include "protegon/timer.h"
#include "utility/time.h"

namespace ptgn {

struct Lifetime {
	ecs::Entity entity;
	milliseconds duration{ 0 };
	Timer timer;
};

} // namespace ptgn