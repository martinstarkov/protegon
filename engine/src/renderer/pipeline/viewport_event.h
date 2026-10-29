#pragma once

#include "core/math/vector2.h"

namespace ptgn::event {

/// @brief Triggered when the logical size is changed.
struct LogicalResized {
	V2_int size;
};

/// @brief Triggered when the display size changes (due to presentation resize or logical size
/// change).
struct DisplayResized {
	V2_int size;
};

/// @brief Triggered when the presentation area resizes.
struct PresentationResized {
	V2_int size;
};

} // namespace ptgn::event