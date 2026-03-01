#include "core/math/easing.h"

#include <cmath>
#include <ostream>
#include <utility>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/math_utils.h"

namespace ptgn {

float ApplyEase(float t, Ease ease) {
	PTGN_ASSERT(t >= 0.0f && t <= 1.0f, "Ease parameter t out of range");

	switch (ease) {
		using enum Ease;
		case Linear:	return t;
		case None:		return 1.0f;
		case InOutSine: return -(std::cos(pi<float> * t) - 1.0f) / 2.0f;
		case InOutQuad:
			return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
		case InOutCubic:
			return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
		case InOutQuart:
			return t < 0.5f ? 8.0f * t * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 4.0f) / 2.0f;
		case InOutQuint:
			return t < 0.5f ? 16.0f * t * t * t * t * t
							: 1.0f - std::pow(-2.0f * t + 2.0f, 5.0f) / 2.0f;
		case InOutExpo:
			if (t == 0.0f) {
				return 0.0f;
			}
			if (t == 1.0f) {
				return 1.0f;
			}
			return t < 0.5f ? std::pow(2.0f, 20.0f * t - 10.0f) / 2.0f
							: (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) / 2.0f;
		case InOutCirc:
			return t < 0.5f ? (1.0f - sqrtf(1.0f - 4.0f * t * t)) / 2.0f
							: (sqrtf(1.0f - std::pow(-2.0f * t + 2.0f, 2.0f)) + 1.0f) / 2.0f;
		case InOutElastic: {
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
		case InOutBack: {
			constexpr float c1 = 1.70158f;
			constexpr float c2 = c1 * 1.525f;
			return t < 0.5f
					 ? (std::pow(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f
					 : (std::pow(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) +
						2.0f) /
						   2.0f;
		}
		case InOutBounce:
			return t < 0.5f ? (1.0f - ApplyEase(1.0f - 2.0f * t, OutBounce)) * 0.5f
							: (1.0f + ApplyEase(2.0f * t - 1.0f, OutBounce)) * 0.5f;
		case InSine:	return 1.0f - std::cos(t * half_pi<float>);
		case OutSine:	return std::sin(t * half_pi<float>);
		case InQuad:	return t * t;
		case OutQuad:	return 1.0f - (1.0f - t) * (1.0f - t);
		case InCubic:	return t * t * t;
		case OutCubic:	return 1.0f - std::pow(1.0f - t, 3.0f);
		case InQuart:	return t * t * t * t;
		case OutQuart:	return 1.0f - std::pow(1.0f - t, 4.0f);
		case InQuint:	return t * t * t * t * t;
		case OutQuint:	return 1.0f - std::pow(1.0f - t, 5.0f);
		case InExpo:	return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f);
		case OutExpo:	return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
		case InCirc:	return 1.0f - sqrtf(1.0f - t * t);
		case OutCirc:	return sqrtf(1.0f - std::pow(t - 1.0f, 2.0f));
		case InElastic: {
			constexpr float c4 = two_pi<float> / 3.0f;
			if (t == 0.0f) {
				return 0.0f;
			}
			if (t == 1.0f) {
				return 1.0f;
			}
			return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
		}
		case OutElastic: {
			constexpr float c4 = two_pi<float> / 3.0f;
			if (t == 0.0f) {
				return 0.0f;
			}
			if (t == 1.0f) {
				return 1.0f;
			}
			return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
		}
		case InBack: {
			constexpr float c1 = 1.70158f;
			constexpr float c3 = c1 + 1.0f;
			return c3 * t * t * t - c1 * t * t;
		}
		case OutBack: {
			constexpr float c1 = 1.70158f;
			constexpr float c3 = c1 + 1.0f;
			return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
		}
		case InBounce:	return 1.0f - ApplyEase(1.0f - t, OutBounce);
		case OutBounce: {
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
		default: PTGN_ERROR("Unknown Ease: ", std::to_underlying(ease));
	}
}

std::ostream& operator<<(std::ostream& os, Ease ease) {
	switch (ease) {
		using enum Ease;
		case Invalid:	   return os << "Invalid";
		case None:		   return os << "None";
		case Linear:	   return os << "Linear";
		case InOutSine:	   return os << "InOutSine";
		case InOutQuad:	   return os << "InOutQuad";
		case InOutCubic:   return os << "InOutCubic";
		case InOutQuart:   return os << "InOutQuart";
		case InOutQuint:   return os << "InOutQuint";
		case InOutExpo:	   return os << "InOutExpo";
		case InOutCirc:	   return os << "InOutCirc";
		case InOutElastic: return os << "InOutElastic";
		case InOutBack:	   return os << "InOutBack";
		case InOutBounce:  return os << "InOutBounce";
		case InSine:	   return os << "InSine";
		case OutSine:	   return os << "OutSine";
		case InQuad:	   return os << "InQuad";
		case OutQuad:	   return os << "OutQuad";
		case InCubic:	   return os << "InCubic";
		case OutCubic:	   return os << "OutCubic";
		case InQuart:	   return os << "InQuart";
		case OutQuart:	   return os << "OutQuart";
		case InQuint:	   return os << "InQuint";
		case OutQuint:	   return os << "OutQuint";
		case InExpo:	   return os << "InExpo";
		case OutExpo:	   return os << "OutExpo";
		case InCirc:	   return os << "InCirc";
		case OutCirc:	   return os << "OutCirc";
		case InElastic:	   return os << "InElastic";
		case OutElastic:   return os << "OutElastic";
		case InBack:	   return os << "InBack";
		case OutBack:	   return os << "OutBack";
		case InBounce:	   return os << "InBounce";
		case OutBounce:	   return os << "OutBounce";
		default:		   PTGN_ERROR("Unknown Ease: ", std::to_underlying(ease));
	}
}

} // namespace ptgn