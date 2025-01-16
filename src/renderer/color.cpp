#include "renderer/color.h"

#include "math/rng.h"
#include "math/vector4.h"

namespace ptgn {

V4_float Color::Normalized() const {
	return { static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
			 static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f };
}

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