#include "core/math/angle.h"

#include <nlohmann/json.hpp>

#include "core/math/math_utils.h"
#include "core/math/rng.h"
#include "serialization/json/fwd.h"

namespace ptgn {

void to_json(json& j, const Degrees& angle) {
	j = angle.value;
}

void from_json(const json& j, Degrees& angle) {
	j.get_to(angle.value);
}

void to_json(json& j, const Radians& angle) {
	j = angle.value;
}

void from_json(const json& j, Radians& angle) {
	j.get_to(angle.value);
}

Degrees Degrees::Random() {
	static RNG<float> rng{ 0.0f, 360.0f };
	return Degrees{ rng() };
}

Degrees Degrees::Random(Degrees min, Degrees max) {
	RNG<float> rng{ min.value, max.value };
	return Degrees{ rng() };
}

Radians Radians::Random() {
	static RNG<float> rng{ 0.0f, kTwoPi };
	return Radians{ rng() };
}

Radians Radians::Random(Radians min, Radians max) {
	RNG<float> rng{ min.value, max.value };
	return Radians{ rng() };
}

} // namespace ptgn