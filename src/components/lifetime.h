#pragma once

#include "protegon/timer.h"
#include "utility/time.h"

namespace ptgn {

struct Lifetime {
	milliseconds duration{ 0 };
	Timer timer;
};

} // namespace ptgn