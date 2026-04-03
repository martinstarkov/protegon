#include "core/math/math_utils.h"

#include <cmath>
#include <cstdlib>
#include <tuple>

#include "core/assert.h"
#include "core/math/tolerance.h"

namespace ptgn {

std::tuple<bool, float, float> QuadraticFormula(float a, float b, float c) {
	const float disc{ b * b - 4.0f * a * c };
	if (disc < 0.0f) {
		// Imaginary roots.
		return { false, 0.0f, 0.0f };
	} else if (NearlyEqual(disc, 0.0f)) {
		// Repeated roots.
		const float root{ -0.5f * b / a };
		return { true, root, root };
	}
	// Real roots.
	const float q = (b > 0.0f) ? -0.5f * (b + std::sqrt(disc)) : -0.5f * (b - std::sqrt(disc));
	// This may look weird but the algebra checks out here (I checked).
	return { true, q / a, c / q };
}

float TriangleWave(float t, float period, float phase_shift) {
	PTGN_ASSERT(period != 0.0f, "Triangle wave period can not be 0");

	t += phase_shift + 0.25f;
	t /= period;

	return 2.0f * std::abs(2.0f * (t - FastRound(t))) - 1.0f;
}

float Quintic(float t) {
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float QuinticInterpolate(float a, float b, float t) {
	return Lerp(a, b, Quintic(t));
}

float Smoothstep(float t) {
	return t * t * (3.0f - 2.0f * t);
}

float SmoothstepInterpolate(float a, float b, float t) {
	/// From: https://en.wikipedia.org/wiki/Smoothstep
	return Lerp(a, b, Smoothstep(t));
}

} // namespace ptgn