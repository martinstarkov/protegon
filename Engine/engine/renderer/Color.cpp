#include "Color.h"

#include <SDL.h>

namespace engine {

Color::Color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) : r{ r }, g{ g }, b{ b }, a{ a } {}

// Source: https://stackoverflow.com/questions/51004638/how-to-get-uint8-t-data-of-uint32-t
Color::Color(std::uint32_t color) : r{ (color >> (8 * 0)) & 0xff }, g{ (color >> (8 * 1)) & 0xff }, b{ (color >> (8 * 2)) & 0xff }, a{ (color >> (8 * 3)) & 0xff } {}

Color Color::RandomSolid() {
	return Color{ static_cast<std::uint8_t>(engine::math::GetRandomValue<int>(0, 255)), static_cast<std::uint8_t>(engine::math::GetRandomValue<int>(0, 255)), static_cast<std::uint8_t>(engine::math::GetRandomValue<int>(0, 255)), 255 };
}

Color Color::Random() {
	return Color{ static_cast<std::uint8_t>(engine::math::GetRandomValue<int>(0, 255)), static_cast<std::uint8_t>(engine::math::GetRandomValue<int>(0, 255)), static_cast<std::uint8_t>(engine::math::GetRandomValue<int>(0, 255)), static_cast<std::uint8_t>(engine::math::GetRandomValue<int>(0, 255)) };
}

Color::operator SDL_Color() const {
	return SDL_Color{ r, g, b, a };
}

std::ostream& operator<<(std::ostream & os, const Color& color) {
	os << '[' << static_cast<unsigned int>(color.r) << ',' << static_cast<unsigned int>(color.g) << ',' << static_cast<unsigned int>(color.b) << ',' << static_cast<unsigned int>(color.a) << ']';
	return os;
}

} // namespace engine