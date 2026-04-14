#pragma once

#include "core/math/vector2.h"

namespace ptgn::event {

/// @brief Triggered when the game size is changed.
struct GameResized {
	V2_int size;
};

/// @brief Triggered when the display size changes (due to window resize or game size change).
struct DisplayResized {
	V2_int size;
};

} // namespace ptgn::event