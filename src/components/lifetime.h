#pragma once

#include "ecs/ecs.h"
#include "utility/time.h"
#include "utility/timer.h"

namespace ptgn {

struct Lifetime {
	Lifetime(milliseconds duration, bool start = false) : duration{ duration } {
		if (start) {
			timer_.Start();
		}
	}

	// Will restart if lifetime is already running.
	void Start() {
		timer_.Start();
	}

	void Update(ecs::Entity e) const {
		if (timer_.Completed(duration)) {
			e.Destroy();
		}
	}

	milliseconds duration{ 0 };

private:
	Timer timer_;
};

} // namespace ptgn