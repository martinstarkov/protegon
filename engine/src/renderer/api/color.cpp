#include "renderer/api/color.h"

#include <cstdint>

#include "core/assert.h"
#include "math/rng.h"
#include "serialization/json/json.h"

namespace ptgn {

Color Color::RandomOpaque() {
	RNG<int> rng{ 0, 255 };
	return { static_cast<std::uint8_t>(rng()), static_cast<std::uint8_t>(rng()),
			 static_cast<std::uint8_t>(rng()), 255 };
}

Color Color::RandomTransparent() {
	RNG<int> rng{ 0, 255 };
	return { static_cast<std::uint8_t>(rng()), static_cast<std::uint8_t>(rng()),
			 static_cast<std::uint8_t>(rng()), static_cast<std::uint8_t>(rng()) };
}

void to_json(json& j, const Color& color) {
	j = json::array({ color.r, color.g, color.b, color.a });
}

void from_json(const json& j, Color& color) {
	PTGN_ASSERT(j.is_array(), "Deserializing a Color from json requires an array");
	PTGN_ASSERT(
		j.size() == 4, "Deserializing a Color from json requires an array with four elements"
	);
	PTGN_ASSERT(j[0].is_number_unsigned(), "Color array elements must be unsigned integers");
	PTGN_ASSERT(j[1].is_number_unsigned(), "Color array elements must be unsigned integers");
	PTGN_ASSERT(j[2].is_number_unsigned(), "Color array elements must be unsigned integers");
	PTGN_ASSERT(j[3].is_number_unsigned(), "Color array elements must be unsigned integers");
	PTGN_ASSERT(j[0] >= 0 && j[0] <= 255, "Color value outside of range [0, 255]");
	PTGN_ASSERT(j[1] >= 0 && j[1] <= 255, "Color value outside of range [0, 255]");
	PTGN_ASSERT(j[2] >= 0 && j[2] <= 255, "Color value outside of range [0, 255]");
	PTGN_ASSERT(j[3] >= 0 && j[3] <= 255, "Color value outside of range [0, 255]");
	color.r = j[0];
	color.g = j[1];
	color.b = j[2];
	color.a = j[3];
}

} // namespace ptgn