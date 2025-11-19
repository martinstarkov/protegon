#pragma once

#include <variant>

#include "serialization/json/enum.h"

namespace ptgn {

enum class SymmetricalEase {
	Invalid = -1,
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
	Invalid = -1,
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

PTGN_SERIALIZE_ENUM(
	SymmetricalEase, { { SymmetricalEase::Invalid, nullptr },
					   { SymmetricalEase::None, "none" },
					   { SymmetricalEase::Linear, "linear" },
					   { SymmetricalEase::InOutSine, "in_out_sine" },
					   { SymmetricalEase::InOutQuad, "in_out_quad" },
					   { SymmetricalEase::InOutCubic, "in_out_cubic" },
					   { SymmetricalEase::InOutQuart, "in_out_quart" },
					   { SymmetricalEase::InOutQuint, "in_out_quint" },
					   { SymmetricalEase::InOutExpo, "in_out_expo" },
					   { SymmetricalEase::InOutCirc, "in_out_circ" },
					   { SymmetricalEase::InOutElastic, "in_out_elastic" },
					   { SymmetricalEase::InOutBack, "in_out_back" },
					   { SymmetricalEase::InOutBounce, "in_out_bounce" } }
);

PTGN_SERIALIZE_ENUM(AsymmetricalEase, { { AsymmetricalEase::Invalid, nullptr },
												  { AsymmetricalEase::InSine, "in_sine" },
												  { AsymmetricalEase::OutSine, "out_sine" },
												  { AsymmetricalEase::InQuad, "in_quad" },
												  { AsymmetricalEase::OutQuad, "out_quad" },
												  { AsymmetricalEase::InCubic, "in_cubic" },
												  { AsymmetricalEase::OutCubic, "out_cubic" },
												  { AsymmetricalEase::InQuart, "in_quart" },
												  { AsymmetricalEase::OutQuart, "out_quart" },
												  { AsymmetricalEase::InQuint, "in_quint" },
												  { AsymmetricalEase::OutQuint, "out_quint" },
												  { AsymmetricalEase::InExpo, "in_expo" },
												  { AsymmetricalEase::OutExpo, "out_expo" },
												  { AsymmetricalEase::InCirc, "in_circ" },
												  { AsymmetricalEase::OutCirc, "out_circ" },
												  { AsymmetricalEase::InElastic, "in_elastic" },
												  { AsymmetricalEase::OutElastic, "out_elastic" },
												  { AsymmetricalEase::InBack, "in_back" },
												  { AsymmetricalEase::OutBack, "out_back" },
												  { AsymmetricalEase::InBounce, "in_bounce" },
												  { AsymmetricalEase::OutBounce, "out_bounce" } });

} // namespace ptgn