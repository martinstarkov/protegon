#pragma once

#include "core/util/time.h"
#include "core/util/timer.h"
#include "runtime/ecs/entity.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;

struct Lifetime {
	Lifetime() = default;

	explicit Lifetime(milliseconds lifetime, bool start = false);

	/// @brief Will restart if lifetime is already running.
	void Start();

	void Update(Entity entity, secondsf dt);

	milliseconds duration{ 0 };

	PTGN_SERIALIZE(Lifetime, duration, timer_)

private:
	friend class Scene;

	static void Update(Scene& scene, secondsf dt);

	ManualTimer timer_;
};

} // namespace ptgn