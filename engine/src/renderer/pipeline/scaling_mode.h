#pragma once

#include <ostream>
#include <utility>

#include "core/log.h"
#include "serialization/serialize.h"

namespace ptgn {

/// @brief How the game size is scaled to the window size, resulting in display size
enum class ScalingMode {
	/// @brief There is no scaling in effect
	Disabled,
	/// @brief The rendered content is stretched to the window size
	Stretch,
	/// @brief The rendered content is fit to the largest dimension and the other dimension is
	/// letterboxed with black bars
	Letterbox,
	/// @brief The rendered content is fit to the smallest dimension and the other dimension extends
	/// beyond the window bounds
	Overscan,
	/// @brief The rendered content is scaled up by integer multiples to fit the window size
	IntegerScale,
};

inline std::ostream& operator<<(std::ostream& os, ScalingMode mode) {
	switch (mode) {
		using enum ScalingMode;
		case Disabled:	   return os << "Disabled";
		case Stretch:	   return os << "Stretch";
		case Letterbox:	   return os << "Letterbox";
		case Overscan:	   return os << "Overscan";
		case IntegerScale: return os << "IntegerScale";
		default:		   PTGN_ERROR("Unknown ScalingMode: ", std::to_underlying(mode));
	}
}

PTGN_SERIALIZE_ENUM(
	ScalingMode, { { ScalingMode::Disabled, "disabled" },
				   { ScalingMode::Stretch, "stretch" },
				   { ScalingMode::Letterbox, "letterbox" },
				   { ScalingMode::Overscan, "overscan" },
				   { ScalingMode::IntegerScale, "integer_scale" } }
);

} // namespace ptgn