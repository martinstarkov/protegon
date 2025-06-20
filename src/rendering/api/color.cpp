#include "rendering/api/color.h"

#include <cstdint>

#include "math/rng.h"
#include "math/vector4.h"

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

} // namespace ptgn