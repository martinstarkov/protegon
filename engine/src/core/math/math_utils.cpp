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

} // namespace ptgn