#include "Color.h"

#include <SDL.h>

#include "math/RNG.h"

namespace engine {

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

Color::Color(const SDL_Color& color) : r{ color.r }, g{ color.g }, b{ color.b }, a{ color.a } {}

Color::Color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) : 
	r{ r }, g{ g }, b{ b }, a{ a } 
{}

// Source: https://stackoverflow.com/questions/51004638/how-to-get-uint8-t-data-of-uint32-t
Color::Color(std::uint32_t color) : 
	r{ color & 255 }, 
	g{ (color >> 8) & 255 }, 
	b{ (color >> 16) & 0xff }, 
	a{ (color >> 24) & 255 } 
{}

Color::operator SDL_Color() const {
	return SDL_Color{ r, g, b, a };
}

std::ostream& operator<<(std::ostream& os, const Color& color) {
	os << '[' << static_cast<unsigned int>(color.r);
	os << ',' << static_cast<unsigned int>(color.g);
	os << ',' << static_cast<unsigned int>(color.b);
	os << ',' << static_cast<unsigned int>(color.a) << ']';
	return os;
}

std::uint32_t Color::ToUint32() const {
	return r + (g << 8) + (b << 16) + (a << 24);
}

bool Color::IsTransparent() const { 
	return a == 0; 
};

} // namespace engine