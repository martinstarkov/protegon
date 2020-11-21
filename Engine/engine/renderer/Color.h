#pragma once

#include <cstdint>
#include <iostream>

#include "utils/Math.h"

struct SDL_Color;

namespace engine {

struct Color {
	Color() = default;
	~Color() = default;
	Color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);
	Color(std::uint32_t color);
	Color(const Color& copy) = default;
	Color(Color&& move) = default;
	Color& operator=(const Color& copy) = default;
	Color& operator=(Color&& move) = default;
	static Color RandomSolid();
	static Color Random();
	operator SDL_Color() const;
	friend std::ostream& operator<<(std::ostream& os, const Color& color);
	std::uint8_t r = 0, g = 0, b = 0, a = 0;
	bool IsTransparent() const { return a == 0; };
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