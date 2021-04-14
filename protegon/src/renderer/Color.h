#pragma once

#include <cstdint>
#include <iostream>

#include "math/Functions.h"

struct SDL_Color;

namespace engine {

class Color {
public:
	std::uint8_t r{ 0 };
	std::uint8_t g{ 0 };
	std::uint8_t b{ 0 };
	std::uint8_t a{ 0 };
	Color() = default;
	~Color() = default;
	Color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);
	Color(std::uint32_t color);
	Color(const Color& copy) = default;
	Color(Color&& move) = default;
	Color& operator=(const Color& copy) = default;
	Color& operator=(Color&& move) = default;
	std::uint32_t ToUint32() const;
	static Color RandomSolid();
	static Color Random();
	operator SDL_Color() const;
	friend std::ostream& operator<<(std::ostream& os, const Color& color);
	bool IsTransparent() const;
};

template <typename U>
inline Color Lerp(const Color& A, const Color& B, U amount) {
	return { engine::math::Lerp<std::uint8_t>(A.r, B.r, amount), engine::math::Lerp<std::uint8_t>(A.g, B.g, amount), engine::math::Lerp<std::uint8_t>(A.b, B.b, amount), engine::math::Lerp<std::uint8_t>(A.a, B.a, amount) };
}

inline bool operator==(const Color& lhs, const Color& rhs) {
	return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

inline bool operator!=(const Color& lhs, const Color& rhs) {
	return !(lhs == rhs);
}

namespace colors {

inline const Color TRANSPARENT{ 0, 0, 0, 0 };
inline const Color COLORLESS{ TRANSPARENT };
inline const Color WHITE{ 255, 255, 255, 255 };
inline const Color BLACK{ 0, 0, 0, 255 };
inline const Color RED{ 255, 0, 0, 255 };
inline const Color DARK_RED{ 128, 0, 0, 255 };
inline const Color ORANGE{ 255, 165, 0, 255 };
inline const Color YELLOW{ 255, 255, 0, 255 };
inline const Color GOLD{ 255, 215, 0, 255 };
inline const Color GREEN{ 0, 128, 0, 255 };
inline const Color LIME{ 0, 255, 0, 255 };
inline const Color DARK_GREEN{ 0, 100, 0, 255 };
inline const Color BLUE{ 0, 0, 255, 255 };
inline const Color DARK_BLUE{ 0, 0, 128, 255 };
inline const Color CYAN{ 0, 255, 255, 255 };
inline const Color TEAL{ 0, 128, 128, 255 };
inline const Color MAGENTA{ 255, 0, 255, 255 };
inline const Color PURPLE{ 128, 0, 128, 255 };
inline const Color PINK{ 255, 192, 203, 255 };
inline const Color GREY{ 128, 128, 128, 255 };
inline const Color SILVER{ 192, 192, 192, 255 };

} // namespace colors

} // namespace engine