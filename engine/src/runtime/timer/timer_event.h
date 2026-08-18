#pragma once

#include <cstdint>

#include "runtime/timer/timer.h"

namespace ptgn::event {

struct TimerElapsed {
	TimerKey timer;

	// The elapsed segment crossed by this update or explicit advance.
	millisecondsf previous_elapsed{ 0.0f };
	millisecondsf elapsed{ 0.0f };
	millisecondsf duration{ 0.0f };
	std::uint64_t count{ 0 };
	bool completed{ true };

	PTGN_REFLECT(
		TimerElapsed,
		timer,
		previous_elapsed,
		elapsed,
		duration,
		count,
		completed
	)
};

} // namespace ptgn::event
