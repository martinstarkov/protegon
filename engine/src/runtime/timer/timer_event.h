#pragma once

#include <cstdint>

#include "runtime/timer/timer.h"

namespace ptgn::event {

struct TimerElapsed {
	TimerKey timer;
	std::uint64_t count{ 0 };

	PTGN_REFLECT(TimerElapsed, timer, count)
};

} // namespace ptgn::event
