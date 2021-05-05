#include "Color.h"

#include <SDL.h>

#include <cassert>

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
Color::Color(std::uint32_t RGBA8888) : 
	r{ RGBA8888 & 255 },
	g{ (RGBA8888 >> 8) & 255 },
	b{ (RGBA8888 >> 16) & 0xff },
	a{ (RGBA8888 >> 24) & 255 }
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

std::uint32_t Color::ToUint32(PixelFormat pixel_format) const {
	assert(
		pixel_format == PixelFormat::ABGR8888 ||
		pixel_format == PixelFormat::ARGB8888 ||
		pixel_format == PixelFormat::BGRA8888 ||
		pixel_format == PixelFormat::RGBA8888 &&
		"Pixel format for using this function must be 8-bit based format"
	);
	switch (pixel_format) {
		case PixelFormat::RGBA8888:
			return r + (g << 8) + (b << 16) + (a << 24);
		case PixelFormat::ABGR8888:
			return a + (b << 8) + (g << 16) + (r << 24);
		case PixelFormat::ARGB8888:
			return a + (r << 8) + (g << 16) + (b << 24);
		case PixelFormat::BGRA8888:
			return b + (g << 8) + (r << 16) + (a << 24);
		default:
			return 0;
	}
	return 0;
}

bool Color::IsTransparent() const { 
	return a == 0; 
};

} // namespace engine