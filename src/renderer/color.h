#pragma once

#include <cstdint>
#include <iosfwd>
#include <ostream>

#include "math/math.h"
#include "math/vector4.h"
#include "utility/type_traits.h"

struct SDL_Color;

namespace ptgn {

enum class BlendMode {
	// Source: https://wiki.libsdl.org/SDL2/SDL_BlendMode

	None  = 0x00000000,	   /*       no blending: dstRGBA = srcRGBA */
	Blend = 0x00000001,	   /*    alpha blending: dstRGB = (srcRGB * srcA) + (dstRGB
							* (1 - srcA)) dstA = srcA + (dstA * (1-srcA)) */
	Add = 0x00000002,	   /* additive blending: dstRGB = (srcRGB * srcA) + dstRGB
													  dstA = dstA */
	Modulate = 0x00000004, /*    color modulate: dstRGB = srcRGB * dstRGB
												 dstA = dstA */
	Multiply = 0x00000008, /*    color multiply: dstRGB = (srcRGB * dstRGB) +
							  (dstRGB * (1 - srcA)) dstA = dstA */
};

struct Color {
	using Type = std::uint8_t;

	Type r{ 0 };
	Type g{ 0 };
	Type b{ 0 };
	Type a{ 0 };

	// Default color is black.
	constexpr Color() = default;

	constexpr Color(Type r, Type g, Type b, Type a) : r{ r }, g{ g }, b{ b }, a{ a } {}

	constexpr Color(const Color& c) : Color{ c.r, c.g, c.b, c.a } {}

	// @return Color values normalized to 0.0f -> 1.0f range.
	[[nodiscard]] Vector4<float> Normalized() const;

	[[nodiscard]] static Color RandomOpaque();
	[[nodiscard]] static Color RandomTransparent();

	[[nodiscard]] friend bool operator==(const Color& lhs, const Color& rhs) {
		return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
	}

	[[nodiscard]] friend bool operator!=(const Color& lhs, const Color& rhs) {
		return !operator==(lhs, rhs);
	}
};

template <typename U, tt::floating_point<U> = true>
[[nodiscard]] inline Color Lerp(const Color& lhs, const Color& rhs, U t) {
	return Color{ static_cast<Color::Type>(Lerp(lhs.r, rhs.r, t)),
				  static_cast<Color::Type>(Lerp(lhs.g, rhs.g, t)),
				  static_cast<Color::Type>(Lerp(lhs.b, rhs.b, t)),
				  static_cast<Color::Type>(Lerp(lhs.a, rhs.a, t)) };
}

template <typename U, tt::floating_point<U> = true>
[[nodiscard]] inline Color Lerp(const Color& lhs, const Color& rhs, U t_r, U t_g, U t_b, U t_a) {
	return Color{ static_cast<Color::Type>(Lerp(lhs.r, rhs.r, t_r)),
				  static_cast<Color::Type>(Lerp(lhs.g, rhs.g, t_g)),
				  static_cast<Color::Type>(Lerp(lhs.b, rhs.b, t_b)),
				  static_cast<Color::Type>(Lerp(lhs.a, rhs.a, t_a)) };
}

namespace color {

inline constexpr Color Transparent{ 0, 0, 0, 0 };
inline constexpr Color Black{ 0, 0, 0, 255 };
inline constexpr Color White{ 255, 255, 255, 255 };
inline constexpr Color Red{ 255, 0, 0, 255 };
inline constexpr Color DarkRed{ 128, 0, 0, 255 };
inline constexpr Color Brown{ 128, 64, 32, 255 };
inline constexpr Color DarkBrown{ 64, 32, 16, 255 };
inline constexpr Color Orange{ 255, 165, 0, 255 };
inline constexpr Color Yellow{ 255, 255, 0, 255 };
inline constexpr Color Gold{ 255, 215, 0, 255 };
inline constexpr Color Green{ 0, 128, 0, 255 };
inline constexpr Color Lime{ 0, 255, 0, 255 };
inline constexpr Color DarkGreen{ 0, 100, 0, 255 };
inline constexpr Color Blue{ 0, 0, 255, 255 };
inline constexpr Color DarkBlue{ 0, 0, 128, 255 };
inline constexpr Color Cyan{ 0, 255, 255, 255 };
inline constexpr Color Teal{ 0, 128, 128, 255 };
inline constexpr Color Magenta{ 255, 0, 255, 255 };
inline constexpr Color Purple{ 128, 0, 128, 255 };
inline constexpr Color Pink{ 255, 192, 203, 255 };
inline constexpr Color LightPink{ 255, 128, 255, 255 };
inline constexpr Color Gray{ 128, 128, 128, 255 };
inline constexpr Color DarkGray{ 64, 64, 64, 255 };
inline constexpr Color LightGray{ 83, 83, 83, 255 };
inline constexpr Color Silver{ 192, 192, 192, 255 };

} // namespace color

inline std::ostream& operator<<(std::ostream& os, const ptgn::Color& color) {
	os << "[";
	os << static_cast<int>(color.r) << ", ";
	os << static_cast<int>(color.g) << ", ";
	os << static_cast<int>(color.b) << ", ";
	os << static_cast<int>(color.a);
	os << "]";
	return os;
}

} // namespace ptgn