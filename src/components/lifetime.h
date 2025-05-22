#pragma once

#include "core/time.h"
#include "core/timer.h"
#include "serialization/serializable.h"


namespace ptgn {

class Entity;

struct Lifetime {
	Lifetime() = default;

	Lifetime(milliseconds lifetime, bool start = false);

	// Will restart if lifetime is already running.
	void Start();

	void Update(Entity& e) const;

	milliseconds duration{ 0 };

	PTGN_SERIALIZER_REGISTER_NAMED(
		Lifetime, KeyValue("duration", duration), KeyValue("timer", timer_)
	)

private:
	Timer timer_;
};

} // namespace ptgn