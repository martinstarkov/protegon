#pragma once

#include <cstdint> // std::uint32_t, std::uint8_t, etc
#include <ostream> // std::ostream

#include "math/Math.h"
#include "renderer/PixelFormat.h"

struct SDL_Color;

namespace ptgn {

class Color {
public:
	// Generates a color with random RGB values and alpha of 255.
	static Color RandomSolid();
	// Generates a color with random RGBA values.
	static Color Random();

	/*
	* Converts color to a 32-bit pixel format.
	* @return The color converted into a 32-bit integer.
	*/
	static constexpr std::uint32_t AsUint32(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
		return r + (g << 8) + (b << 16) + (a << 24);
	}

	// Default construct color to black.
	Color() = default;
	~Color() = default;
	
	// Construct color from individual RGBA8888 pixel format values.
	constexpr Color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) :
		r{ r }, g{ g }, b{ b }, a{ a } {}

	/*
	* Construct color from a pixel format integer.
	* @param pixel Integer from which to retrieve red, green, blue, and alpha values.
	* @param format The format which the pixel follows.
	*/
	Color(std::uint32_t pixel, PixelFormat format);

	Color(const Color& copy) = default;
	Color(Color&& move) = default;
	Color& operator=(const Color& copy) = default;
	Color& operator=(Color&& move) = default;

	// Implicit conversion to SDL_Color, for internal use.
	operator SDL_Color() const;

	friend std::ostream& operator<<(std::ostream& os, const Color& color);

	friend bool operator==(const Color& lhs, const Color& rhs) {
		return
			lhs.r == rhs.r &&
			lhs.g == rhs.g &&
			lhs.b == rhs.b &&
			lhs.a == rhs.a;
	}

	friend bool operator!=(const Color& lhs, const Color& rhs) {
		return !(lhs == rhs);
	}
	
	/*
	* Converts color to a given pixel format integer
	* @param format The format according to which pixels are converted to an integer.
	* @return The color converted to the given pixel format, 0 if invalid pixel format.
	*/
	std::uint32_t ToUint32(PixelFormat format) const;

	/*
	* Converts color to a 32-bit pixel format
	* @return The color converted into a 32-bit integer.
	*/
	constexpr std::uint32_t ToUint32() const {
		return AsUint32(r, g, b, a);
	}

	/*
	* @return True if alpha value of color is 0, false otherwise.
	*/
	bool IsTransparent() const;

	// Default color: black.

	std::uint8_t r{ 0 };
	std::uint8_t g{ 0 };
	std::uint8_t b{ 0 };
	std::uint8_t a{ 255 };

private:
	// Construction from SDL_Color, for internal use.
	Color(const SDL_Color& color);
};

namespace math {

template <typename T,
	type_traits::is_floating_point_e<T> = true>
inline Color Lerp(const Color& a, const Color& b, T t) {
	return {
		Lerp<std::uint8_t>(a.r, b.r, t),
		Lerp<std::uint8_t>(a.g, b.g, t),
		Lerp<std::uint8_t>(a.b, b.b, t),
		Lerp<std::uint8_t>(a.a, b.a, t)
	};
}

} // namespace math

} // namespace ptgn