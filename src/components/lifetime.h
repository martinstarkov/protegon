#pragma once

#include "core/time.h"
#include "core/timer.h"
#include "serialization/serializable.h"

namespace ptgn {

class Manager;
class Entity;

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

	static void Update(Manager& manager);

	Timer timer_;
};

} // namespace ptgn