#include "Color.h"

#include <cassert> // assert

#include <SDL.h>

#include "math/RNG.h"

namespace ptgn {

Color Color::RandomSolid() {
	math::RNG<std::uint16_t> rng{ 0, 255 };
	return Color{ 
		static_cast<std::uint8_t>(rng()), 
		static_cast<std::uint8_t>(rng()),
		static_cast<std::uint8_t>(rng()),
		255
	};
}

Color Color::Random() {
	math::RNG<std::uint16_t> rng{ 0, 255 };
	return Color{
		static_cast<std::uint8_t>(rng()),
		static_cast<std::uint8_t>(rng()),
		static_cast<std::uint8_t>(rng()),
		static_cast<std::uint8_t>(rng())
	};
}

Color::Color(const SDL_Color& color) : r{ color.r }, g{ color.g }, b{ color.b }, a{ color.a } {}

Color::Color(std::uint32_t pixel, PixelFormat format) {
	SDL_GetRGBA(pixel, format, &r, &g, &b, &a);
}

Color::operator SDL_Color() const {
	return SDL_Color{ r, g, b, a };
}

std::uint32_t Color::ToUint32(PixelFormat format) const {
	return SDL_MapRGBA(format, r, g, b, a);
}

} // namespace ptgn