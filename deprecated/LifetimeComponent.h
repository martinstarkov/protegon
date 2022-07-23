#pragma once

#include "utils/Countdown.h"
#include "utils/TypeTraits.h"

namespace ptgn {

struct LifetimeComponent {
	LifetimeComponent() = default;
	~LifetimeComponent() = default;
	// Duration by default in milliseconds.
	template <typename Duration = milliseconds, 
		type_traits::is_duration_e<Duration> = true>
	LifetimeComponent(Duration lifetime, bool is_dying = true) : 
		countdown{ lifetime } {
		if (is_dying) {
			countdown.Start();
		}
	}
	Countdown countdown;
};

} // namespace ptgn