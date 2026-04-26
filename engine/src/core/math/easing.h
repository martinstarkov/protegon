#pragma once

#include "serialization/serialize.h"

namespace ptgn {

enum class Ease {
	// Symmetrical eases.
	Linear,
	InOutSine,
	InOutQuad,
	InOutCubic,
	InOutQuart,
	InOutQuint,
	InOutExpo,
	InOutCirc,
	InOutElastic,
	InOutBack,
	InOutBounce,

	// Asymmetrical eases.

	InSine,
	OutSine,
	InQuad,
	OutQuad,
	InCubic,
	OutCubic,
	InQuart,
	OutQuart,
	InQuint,
	OutQuint,
	InExpo,
	OutExpo,
	InCirc,
	OutCirc,
	InElastic,
	OutElastic,
	InBack,
	OutBack,
	InBounce,
	OutBounce,

	// No ease.

	None
};
PTGN_SERIALIZE_ENUM(Ease);

[[nodiscard]] bool IsSymmetricalEase(Ease ease);

[[nodiscard]] float ApplyEase(float t, Ease ease);

} // namespace ptgn