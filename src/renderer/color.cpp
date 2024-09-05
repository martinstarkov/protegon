#include "protegon/color.h"

#include "protegon/rng.h"

namespace ptgn {

Vector4<float> Color::Normalized() const {
	return { static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
			 static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f };
}

Color Color::RandomOpaque() {
	RNG<int> rng{ 0, 255 };
	return { static_cast<Color::Type>(rng()), static_cast<Color::Type>(rng()),
			 static_cast<Color::Type>(rng()), 255 };
}

Color Color::RandomTransparent() {
	RNG<int> rng{ 0, 255 };
	return { static_cast<Color::Type>(rng()), static_cast<Color::Type>(rng()),
			 static_cast<Color::Type>(rng()), static_cast<Color::Type>(rng()) };
}

} // namespace ptgn