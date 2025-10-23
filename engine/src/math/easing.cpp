#include "math/easing.h"

#include <cmath>
#include <variant>

#include "debug/core/log.h"
#include "debug/runtime/assert.h"
#include "math/math_utils.h"
#include "serialization/json/json.h"

namespace ptgn {

float ApplyEase(float t, SymmetricalEase ease) {
	PTGN_ASSERT(t >= 0.0f && t <= 1.0f, "Ease parameter t out of range");

	switch (ease) {
		case SymmetricalEase::Linear:	 return t;
		case SymmetricalEase::None:		 return 1.0f;
		case SymmetricalEase::InOutSine: return -(std::cos(pi<float> * t) - 1.0f) / 2.0f;
		case SymmetricalEase::InOutQuad:
			return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
		case SymmetricalEase::InOutCubic:
			return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
		case SymmetricalEase::InOutQuart:
			return t < 0.5f ? 8.0f * t * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 4.0f) / 2.0f;
		case SymmetricalEase::InOutQuint:
			return t < 0.5f ? 16.0f * t * t * t * t * t
							: 1.0f - std::pow(-2.0f * t + 2.0f, 5.0f) / 2.0f;
		case SymmetricalEase::InOutExpo:
			if (t == 0.0f) {
				return 0.0f;
			}
			if (t == 1.0f) {
				return 1.0f;
			}
			return t < 0.5f ? std::pow(2.0f, 20.0f * t - 10.0f) / 2.0f
							: (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) / 2.0f;
		case SymmetricalEase::InOutCirc:
			return t < 0.5f ? (1.0f - sqrtf(1.0f - 4.0f * t * t)) / 2.0f
							: (sqrtf(1.0f - std::pow(-2.0f * t + 2.0f, 2.0f)) + 1.0f) / 2.0f;
		case SymmetricalEase::InOutElastic: {
			constexpr float c5 = two_pi<float> / 4.5f;
			if (t == 0.0f || t == 1.0f) {
				return t;
			}
			return t < 0.5f
					 ? -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) /
						   2.0f
					 : (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) /
							   2.0f +
						   1.0f;
		}
		case SymmetricalEase::InOutBack: {
			constexpr float c1 = 1.70158f;
			constexpr float c2 = c1 * 1.525f;
			return t < 0.5f
					 ? (std::pow(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f
					 : (std::pow(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) +
						2.0f) /
						   2.0f;
		}
		case SymmetricalEase::InOutBounce:
			return t < 0.5f
					 ? (1.0f - ApplyEase(1.0f - 2.0f * t, AsymmetricalEase::OutBounce)) * 0.5f
					 : (1.0f + ApplyEase(2.0f * t - 1.0f, AsymmetricalEase::OutBounce)) * 0.5f;
		default: PTGN_ERROR("Unrecognized symmetrical ease type");
	}
}

float ApplyEase(float t, AsymmetricalEase ease) {
	PTGN_ASSERT(t >= 0.0f && t <= 1.0f, "Ease parameter t out of range");

	switch (ease) {
		case AsymmetricalEase::InSine:	  return 1.0f - std::cos(t * half_pi<float>);
		case AsymmetricalEase::OutSine:	  return std::sin(t * half_pi<float>);
		case AsymmetricalEase::InQuad:	  return t * t;
		case AsymmetricalEase::OutQuad:	  return 1.0f - (1.0f - t) * (1.0f - t);
		case AsymmetricalEase::InCubic:	  return t * t * t;
		case AsymmetricalEase::OutCubic:  return 1.0f - std::pow(1.0f - t, 3.0f);
		case AsymmetricalEase::InQuart:	  return t * t * t * t;
		case AsymmetricalEase::OutQuart:  return 1.0f - std::pow(1.0f - t, 4.0f);
		case AsymmetricalEase::InQuint:	  return t * t * t * t * t;
		case AsymmetricalEase::OutQuint:  return 1.0f - std::pow(1.0f - t, 5.0f);
		case AsymmetricalEase::InExpo:	  return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f);
		case AsymmetricalEase::OutExpo:	  return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
		case AsymmetricalEase::InCirc:	  return 1.0f - sqrtf(1.0f - t * t);
		case AsymmetricalEase::OutCirc:	  return sqrtf(1.0f - std::pow(t - 1.0f, 2.0f));
		case AsymmetricalEase::InElastic: {
			constexpr float c4 = two_pi<float> / 3.0f;
			return t == 0.0f ? 0.0f
				 : t == 1.0f
					 ? 1.0f
					 : -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
		}
		case AsymmetricalEase::OutElastic: {
			constexpr float c4 = two_pi<float> / 3.0f;
			return t == 0.0f ? 0.0f
				 : t == 1.0f
					 ? 1.0f
					 : std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
		}
		case AsymmetricalEase::InBack: {
			constexpr float c1 = 1.70158f;
			constexpr float c3 = c1 + 1.0f;
			return c3 * t * t * t - c1 * t * t;
		}
		case AsymmetricalEase::OutBack: {
			constexpr float c1 = 1.70158f;
			constexpr float c3 = c1 + 1.0f;
			return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
		}
		case AsymmetricalEase::InBounce:
			return 1.0f - ApplyEase(1.0f - t, AsymmetricalEase::OutBounce);
		case AsymmetricalEase::OutBounce: {
			constexpr float n1 = 7.5625f;
			constexpr float d1 = 2.75f;
			if (t < 1.0f / d1) {
				return n1 * t * t;
			} else if (t < 2.0f / d1) {
				t -= 1.5f / d1;
				return n1 * t * t + 0.75f;
			} else if (t < 2.5f / d1) {
				t -= 2.25f / d1;
				return n1 * t * t + 0.9375f;
			} else {
				t -= 2.625f / d1;
				return n1 * t * t + 0.984375f;
			}
		}
		default: PTGN_ERROR("Unrecognized asymmetrical ease type");
	}
}

float ApplyEase(float t, const Ease& ease) {
	return std::visit([t](auto&& e) -> float { return ApplyEase(t, e); }, ease);
}

} // namespace ptgn