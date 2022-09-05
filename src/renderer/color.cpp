#include "protegon/color.h"

#include <SDL.h>

namespace ptgn {

constexpr Color::Color(std::uint8_t r,
				       std::uint8_t g,
				       std::uint8_t b,
				       std::uint8_t a) :
	r{ r }, g{ g }, b{ b }, a{ a } {}

Color::operator SDL_Color() const {
	return SDL_Color{ r, g, b, a };
}

} // namespace ptgn