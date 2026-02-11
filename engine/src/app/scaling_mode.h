#pragma once

#include "serialization/json/enum.h"

namespace ptgn {

// How the game size is scaled to the window size, resulting in display size.
enum class ScalingMode {
	Disabled,	  /* There is no scaling in effect */
	Stretch,	  /* The rendered content is stretched to the window size */
	Letterbox,	  /* The rendered content is fit to the largest dimension and the other dimension is
					 letterboxed with black bars */
	Overscan,	  /* The rendered content is fit to the smallest dimension and the other dimension
					 extends beyond the window bounds */
	IntegerScale, /* The rendered content is scaled up by integer multiples to fit the window size
				   */
};

PTGN_SERIALIZE_ENUM(
	ScalingMode, { { ScalingMode::Disabled, "disabled" },
				   { ScalingMode::Stretch, "stretch" },
				   { ScalingMode::Letterbox, "letterbox" },
				   { ScalingMode::Overscan, "overscan" },
				   { ScalingMode::IntegerScale, "integer_scale" } }
);

} // namespace ptgn