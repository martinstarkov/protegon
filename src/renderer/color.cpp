#include "protegon/color.h"

#include "SDL.h"
#include "protegon/rng.h"

namespace ptgn {

Color::operator SDL_Color() const {
	return SDL_Color{ r, g, b, a };
}

[[nodiscard]] Color Color::RandomOpaque() {
	RNG<int> rng{ 0, 255 };
	return { static_cast<Color::Type>(rng()), static_cast<Color::Type>(rng()),
			 static_cast<Color::Type>(rng()), 255 };
}

[[nodiscard]] Color Color::RandomTransparent() {
	RNG<int> rng{ 0, 255 };
	return { static_cast<Color::Type>(rng()), static_cast<Color::Type>(rng()),
			 static_cast<Color::Type>(rng()), static_cast<Color::Type>(rng()) };
}

} // namespace ptgn