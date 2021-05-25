#pragma once

#include <cstdint> // std::uint32_t, std::uint8_t, etc
#include <ostream> // std::ostream

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

	template <typename T, 
		type_traits::is_floating_point_e<T> = true>
	static Color Lerp(const Color& a, const Color& b, T t) {
		return {
			math::Lerp<std::uint8_t>(a.r, b.r, t), 
			math::Lerp<std::uint8_t>(a.g, b.g, t), 
			math::Lerp<std::uint8_t>(a.b, b.b, t), 
			math::Lerp<std::uint8_t>(a.a, b.a, t) 
		};
	}

	// Default construct color to black.
	Color() = default;

	~Color() = default;
	
	// Construct color from individual RGBA8888 pixel format values.
	Color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);

	/* 
	* Construct color from a pixel format integer.
	* @param color Integer from which to retrieve red, green, blue, and alpha values.
	* @param format The pixel format for conversion (order and size of colors).
	*/
	Color(std::uint32_t color, PixelFormat format = PixelFormat::RGBA8888);

	Color(const Color& copy) = default;
	Color(Color&& move) = default;
	Color& operator=(const Color& copy) = default;
	Color& operator=(Color&& move) = default;

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

	// Default color: black.

	std::uint8_t r{ 0 };
	std::uint8_t g{ 0 };
	std::uint8_t b{ 0 };
	std::uint8_t a{ 255 };

private:
	// Construction from SDL_Color, for internal use.
	Color(const SDL_Color& color);
};

} // namespace engine