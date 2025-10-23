#pragma once

#include "core/utils/time.h"
#include "core/utils/timer.h"
#include "serialization/json/serializable.h"

namespace ptgn {

class Manager;
class Entity;
class Scene;

struct Lifetime {
	Lifetime() = default;

	Lifetime(milliseconds lifetime, bool start = false);

	// Will restart if lifetime is already running.
	void Start();

	void Update(Entity& entity) const;

	milliseconds duration{ 0 };

	PTGN_SERIALIZER_REGISTER_NAMED(
		Lifetime, KeyValue("duration", duration), KeyValue("timer", timer_)
	)

private:
	friend class Scene;

	static void Update(Scene& scene);

	Timer timer_;
};

} // namespace ptgn