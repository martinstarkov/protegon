#pragma once

#include <ostream>

#include "serialization/json/enum.h"

namespace ptgn {

enum class Ease {
	Invalid = -1,

	None,

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
	OutBounce
};

std::ostream& operator<<(std::ostream& os, Ease ease);

[[nodiscard]] float ApplyEase(float t, Ease ease);

PTGN_SERIALIZE_ENUM(
	Ease, { { Ease::Invalid, nullptr },
			{ Ease::None, "none" },
			{ Ease::Linear, "linear" },
			{ Ease::InOutSine, "in_out_sine" },
			{ Ease::InOutQuad, "in_out_quad" },
			{ Ease::InOutCubic, "in_out_cubic" },
			{ Ease::InOutQuart, "in_out_quart" },
			{ Ease::InOutQuint, "in_out_quint" },
			{ Ease::InOutExpo, "in_out_expo" },
			{ Ease::InOutCirc, "in_out_circ" },
			{ Ease::InOutElastic, "in_out_elastic" },
			{ Ease::InOutBack, "in_out_back" },
			{ Ease::InOutBounce, "in_out_bounce" },
			{ Ease::InSine, "in_sine" },
			{ Ease::OutSine, "out_sine" },
			{ Ease::InQuad, "in_quad" },
			{ Ease::OutQuad, "out_quad" },
			{ Ease::InCubic, "in_cubic" },
			{ Ease::OutCubic, "out_cubic" },
			{ Ease::InQuart, "in_quart" },
			{ Ease::OutQuart, "out_quart" },
			{ Ease::InQuint, "in_quint" },
			{ Ease::OutQuint, "out_quint" },
			{ Ease::InExpo, "in_expo" },
			{ Ease::OutExpo, "out_expo" },
			{ Ease::InCirc, "in_circ" },
			{ Ease::OutCirc, "out_circ" },
			{ Ease::InElastic, "in_elastic" },
			{ Ease::OutElastic, "out_elastic" },
			{ Ease::InBack, "in_back" },
			{ Ease::OutBack, "out_back" },
			{ Ease::InBounce, "in_bounce" },
			{ Ease::OutBounce, "out_bounce" } }
);

} // namespace ptgn