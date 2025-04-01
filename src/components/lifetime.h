#pragma once

#include "utility/time.h"
#include "utility/timer.h"

namespace ptgn {

class Entity;

struct Lifetime {
	Lifetime(milliseconds duration, bool start = false);

	// Will restart if lifetime is already running.
	void Start();

	void Update(Entity& e) const;

	milliseconds duration{ 0 };

private:
	Timer timer_;
};

} // namespace ptgn