#pragma once

#include <cstdint>
#include <ostream>
#include <array>

#include "protegon/math.h"
#include "utility/type_traits.h"

struct SDL_Color;

namespace ptgn {

struct Color {
	std::uint8_t r{ 0 };
	std::uint8_t g{ 0 };
	std::uint8_t b{ 0 };
	std::uint8_t a{ 255 };
	operator SDL_Color() const;
	// Default color is black.
	constexpr Color() = default;

	constexpr Color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) :
		r{ r }, g{ g }, b{ b }, a{ a } {}

	// @return Color values normalized to 0.0f -> 1.0f range.
	std::array<float, 4> Normalized() const {
		return std::array<float, 4>{
			static_cast<float>(r) / 255.0f,
			static_cast<float>(g) / 255.0f,
			static_cast<float>(b) / 255.0f,
			static_cast<float>(a) / 255.0f
		};
	}
};

inline bool operator==(const Color& lhs, const Color& rhs) {
	return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

inline bool operator!=(const Color& lhs, const Color& rhs) {
	return !operator==(lhs, rhs);
}

template <typename U, type_traits::floating_point<U> = true>
[[nodiscard]] inline Color Lerp(const Color& lhs, const Color& rhs, U t) {
	return Color{ static_cast<std::uint8_t>(Lerp(lhs.r, rhs.r, t)),
				  static_cast<std::uint8_t>(Lerp(lhs.g, rhs.g, t)),
				  static_cast<std::uint8_t>(Lerp(lhs.b, rhs.b, t)),
				  static_cast<std::uint8_t>(Lerp(lhs.a, rhs.a, t)) };
}

template <typename U, type_traits::floating_point<U> = true>
[[nodiscard]] inline Color Lerp(const Color& lhs, const Color& rhs, U t_r, U t_g, U t_b, U t_a) {
	return Color{ static_cast<std::uint8_t>(Lerp(lhs.r, rhs.r, t_r)),
				  static_cast<std::uint8_t>(Lerp(lhs.g, rhs.g, t_g)),
				  static_cast<std::uint8_t>(Lerp(lhs.b, rhs.b, t_b)),
				  static_cast<std::uint8_t>(Lerp(lhs.a, rhs.a, t_a)) };
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
inline constexpr Color Grey{ 128, 128, 128, 255 };
inline constexpr Color DarkGrey{ 64, 64, 64, 255 };
inline constexpr Color LightGrey{ 83, 83, 83, 255 };
inline constexpr Color Silver{ 192, 192, 192, 255 };

} // namespace color

} // namespace ptgn

inline std::ostream& operator<<(std::ostream& os, const ptgn::Color& color) {
	os << "[";
	os << static_cast<int>(color.r) << ", ";
	os << static_cast<int>(color.g) << ", ";
	os << static_cast<int>(color.b) << ", ";
	os << static_cast<int>(color.a);
	os << "]";
	return os;
}