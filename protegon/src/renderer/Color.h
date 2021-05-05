#pragma once

#include <cstdint>
#include <iostream>

#include "math/Math.h"
#include "renderer/sprites/PixelFormat.h"

struct SDL_Color;

namespace engine {

class Color {
public:
	// Generates a color with random RGB values and alpha of 255.
	static Color RandomSolid();
	// Generates a color with random RGBA values.
	static Color Random();

	// Default construct color to black.
	Color() = default;

	~Color() = default;
	
	// Construct color from individual RGBA8888 pixel format values.
	Color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);

	// Construct color from a combined RGBA8888 pixel format integer.
	Color(std::uint32_t RGBA8888);

	Color(const Color& copy) = default;
	Color(Color&& move) = default;
	Color& operator=(const Color& copy) = default;
	Color& operator=(Color&& move) = default;

	friend std::ostream& operator<<(std::ostream& os, const Color& color);

	// Implicit conversion to SDL_Color, for internal use.
	operator SDL_Color() const;
	
	/*
	* Converts color to a given pixel format integer
	* @param pixel_format The format according to which pixels are converted to an integer.
	* @return The color converted to the given pixel format, 0 if invalid pixel format.
	*/
	std::uint32_t ToUint32(PixelFormat pixel_format = PixelFormat::RGBA8888) const;

	/*
	* @return True if alpha value of color is 0, false otherwise.
	*/
	bool IsTransparent() const;

	std::uint8_t r{ 0 };
	std::uint8_t g{ 0 };
	std::uint8_t b{ 0 };
	std::uint8_t a{ 0 };
private:
	// Construction from SDL_Color, for internal use.
	Color(const SDL_Color& color);
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

inline const Color BLACK{ 0, 0, 0, 255 };
inline const Color WHITE{ 255, 255, 255, 255 };
inline const Color DEFAULT_DRAW_COLOR{ BLACK };
inline const Color DEFAULT_BACKGROUND_COLOR{ WHITE };
inline const Color TRANSPARENT{ 0, 0, 0, 0 };
inline const Color COLORLESS{ TRANSPARENT };
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