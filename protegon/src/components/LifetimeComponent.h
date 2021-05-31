#pragma once

#include "utils/Countdown.h"

namespace engine {

struct LifetimeComponent {
	LifetimeComponent() = default;
	LifetimeComponent(milliseconds lifetime, bool is_dying = true) : 
		countdown{ lifetime } {
		if (is_dying) {
			countdown.Start();
		}
	}
	Countdown countdown;
};

} // namespace engine