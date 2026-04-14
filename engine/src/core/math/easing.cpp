#include "core/math/easing.h"

#include <cmath>
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
		case InOutSine: return -(std::cos(kPi * t) - 1.0f) / 2.0f;
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
			constexpr float c5 = kTwoPi / 4.5f;
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
		case InSine:	return 1.0f - std::cos(t * kHalfPi);
		case OutSine:	return std::sin(t * kHalfPi);
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
			constexpr float c4 = kTwoPi / 3.0f;
			if (t == 0.0f) {
				return 0.0f;
			}
			if (t == 1.0f) {
				return 1.0f;
			}
			return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
		}
		case OutElastic: {
			constexpr float c4 = kTwoPi / 3.0f;
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

bool IsSymmetricalEase(Ease ease) {
	switch (ease) {
		using enum Ease;
		case Linear:
		case InOutSine:
		case InOutQuad:
		case InOutCubic:
		case InOutQuart:
		case InOutQuint:
		case InOutExpo:
		case InOutCirc:
		case InOutElastic:
		case InOutBack:
		case InOutBounce:  return true;
		default:		   return false;
	}
}

} // namespace ptgn