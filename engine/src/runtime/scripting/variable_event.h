#pragma once

#include <cstdint>

#include "runtime/scripting/variables.h"

namespace ptgn::event {

struct CounterChanged {
	CounterKey counter{};
	std::int64_t previous_value{ 0 };
	std::int64_t value{ 0 };
	PTGN_REFLECT(CounterChanged, counter, previous_value, value)
};

struct BooleanChanged {
	BooleanKey boolean{};
	bool previous_value{ false };
	bool value{ false };
	PTGN_REFLECT(BooleanChanged, boolean, previous_value, value)
};

} // namespace ptgn::event
