#pragma once

#include <cstdint>
#include <iostream>

#include <engine/utils/Math.h>

#include <SDL.h>

namespace engine {

struct Color {
	Color() = default;
	Color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) : r{ r }, g{ g }, b{ b }, a{ a } {}
	~Color() = default;
	static Color RandomSolid() {
		return Color{ static_cast<std::uint8_t>(engine::math::GetRandomValue<int>(0, 255)), static_cast<std::uint8_t>(engine::math::GetRandomValue<int>(0, 255)), static_cast<std::uint8_t>(engine::math::GetRandomValue<int>(0, 255)), 255 };
	}
	static Color Random() {
		return Color{ static_cast<std::uint8_t>(engine::math::GetRandomValue<int>(0, 255)), static_cast<std::uint8_t>(engine::math::GetRandomValue<int>(0, 255)), static_cast<std::uint8_t>(engine::math::GetRandomValue<int>(0, 255)), static_cast<std::uint8_t>(engine::math::GetRandomValue<int>(0, 255)) };
	}
	operator SDL_Color() const {
		return SDL_Color{ r, g, b, a };
	}
	friend std::ostream& operator<<(std::ostream& os, Color& color) {
		os << '[' << unsigned(color.r) << ',' << unsigned(color.g) << ',' << unsigned(color.b) << ',' << unsigned(color.a) << ']';
		return os;
	}
	std::uint8_t r = 0, g = 0, b = 0, a = 0;
};

inline bool operator==(const Color& lhs, const Color& rhs) {
	return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

inline bool operator!=(const Color& lhs, const Color& rhs) {
	return !(lhs == rhs);
}

#define TRANSPARENT Color{ 0, 0, 0, 0 }
#define COLORLESS TRANSPARENT
#define WHITE Color{ 255, 255, 255, 255 }
#define BLACK Color{ 0, 0, 0, 255 }
#define RED Color{ 255, 0, 0, 255 }
#define DARK_RED Color{ 128, 0, 0, 255 }
#define ORANGE Color{ 255, 165, 0, 255 }
#define YELLOW Color{ 255, 255, 0, 255 }
#define GOLD Color{ 255, 215, 0, 255 }
#define GREEN Color{ 0, 128, 0, 255 }
#define LIME Color{ 0, 255, 0, 255 }
#define DARK_GREEN Color{ 0, 100, 0, 255 }
#define BLUE Color{ 0, 0, 255, 255 }
#define DARK_BLUE Color{ 0, 0, 128, 255 }
#define CYAN Color{ 0, 255, 255, 255 }
#define TEAL Color{ 0, 128, 128, 255 }
#define MAGENTA Color{ 255, 0, 255, 255 }
#define PURPLE Color{ 128, 0, 128, 255 }
#define PINK Color{ 255, 192, 203, 255 }
#define GREY Color{ 128, 128, 128, 255 }
#define SILVER Color{ 192, 192, 192, 255 }

} // namespace engine