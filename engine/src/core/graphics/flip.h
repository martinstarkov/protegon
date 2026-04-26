#pragma once

#include "serialization/serialize.h"

namespace ptgn {

/// @brief Specifies 2D axis mirroring operations.
///
/// Values are bitmask-compatible and may represent horizontal,
/// vertical, or combined flipping.
enum class Flip {
	None	   = 0,
	Horizontal = 1,
	Vertical   = 2,
	Both	   = 3
};
PTGN_SERIALIZE_ENUM(Flip);

} // namespace ptgn