#pragma once

#include <ostream>
#include <utility>

#include "core/log.h"
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

inline std::ostream& operator<<(std::ostream& os, Flip flip) {
	switch (flip) {
		using enum Flip;
		case None:		 return os << "None";
		case Horizontal: return os << "Horizontal";
		case Vertical:	 return os << "Vertical";
		case Both:		 return os << "Both";
		default:		 PTGN_ERROR("Unknown Flip: ", std::to_underlying(flip));
	}
}

PTGN_REFLECT_ENUM(
	Flip, { { Flip::None, "none" },
			{ Flip::Horizontal, "horizontal" },
			{ Flip::Vertical, "vertical" },
			{ Flip::Both, "both" } }
);

} // namespace ptgn