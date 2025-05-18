#pragma once

#include <variant>

namespace ptgn {

enum class SymmetricalEase {
	None,
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
	InOutBounce
};

enum class AsymmetricalEase {
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

using Ease = std::variant<SymmetricalEase, AsymmetricalEase>;

[[nodiscard]] float ApplyEase(float t, SymmetricalEase ease);

[[nodiscard]] float ApplyEase(float t, AsymmetricalEase ease);

[[nodiscard]] float ApplyEase(float t, const Ease& ease);

} // namespace ptgn