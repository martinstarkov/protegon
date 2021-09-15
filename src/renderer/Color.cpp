#include "Color.h"

#include <SDL.h>
#include <cassert> // assert

#include "math/RNG.h"

namespace ptgn {

Color Color::RandomSolid() {
	RNG<std::uint16_t> rng{ 0, 255 };
	return Color{ 
		static_cast<std::uint8_t>(rng()), 
		static_cast<std::uint8_t>(rng()),
		static_cast<std::uint8_t>(rng()),
		255
	};
}

Color Color::Random() {
	RNG<std::uint16_t> rng{ 0, 255 };
	return Color{
		static_cast<std::uint8_t>(rng()),
		static_cast<std::uint8_t>(rng()),
		static_cast<std::uint8_t>(rng()),
		static_cast<std::uint8_t>(rng())
	};
}

Color::Color(const SDL_Color& color) : 
	r{ color.r }, g{ color.g }, b{ color.b }, a{ color.a } 
{}

Color::Color(std::uint32_t pixel, PixelFormat format) {
	SDL_GetRGBA(pixel, format, &r, &g, &b, &a);
}

Color::operator SDL_Color() const {
	return SDL_Color{ r, g, b, a };
}

std::ostream& operator<<(std::ostream& os, const Color& color) {
	os << '[' << static_cast<std::uint32_t>(color.r);
	os << ',' << static_cast<std::uint32_t>(color.g);
	os << ',' << static_cast<std::uint32_t>(color.b);
	os << ',' << static_cast<std::uint32_t>(color.a) << ']';
	return os;
}

std::uint32_t Color::ToUint32(PixelFormat format) const {
	return SDL_MapRGBA(format, r, g, b, a);
}

bool Color::IsTransparent() const { 
	return a == 0; 
};

} // namespace ptgn