#pragma once

#include <cstdint>

#include "runtime/timer/timer.h"

namespace ptgn::event {

struct TimerElapsed {
	TimerKey timer{};

	// The elapsed segment crossed by this update or explicit advance.
	milliseconds previous_elapsed{ 0 };
	milliseconds elapsed{ 0 };
	milliseconds duration{ 0 };
	std::uint64_t count{ 0 };
	bool completed{ true };

	PTGN_REFLECT(TimerElapsed, timer, previous_elapsed, elapsed, duration, count, completed)
};

} // namespace ptgn::event
