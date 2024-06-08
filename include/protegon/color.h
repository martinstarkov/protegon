#pragma once

#include "protegon/type_traits.h"

#include <cstdint>

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
	constexpr Color(std::uint8_t r,
                    std::uint8_t g,
                    std::uint8_t b,
                    std::uint8_t a) :
		r{ r }, g{ g }, b{ b }, a{ a } {}
};

inline bool operator==(const Color& lhs, const Color& rhs) {
	return
		lhs.r == rhs.r &&
		lhs.g == rhs.g &&
		lhs.b == rhs.b &&
		lhs.a == rhs.a;
}

inline bool operator!=(const Color& lhs, const Color& rhs) {
	return !operator==(lhs, rhs);
}

template <typename U, type_traits::floating_point<U> = true>
[[nodiscard]] inline Color Lerp(const Color& lhs, const Color& rhs, U t) {
	return Color{
		static_cast<std::uint8_t>(Lerp(lhs.r, rhs.r, t)),
		static_cast<std::uint8_t>(Lerp(lhs.g, rhs.g, t)),
		static_cast<std::uint8_t>(Lerp(lhs.b, rhs.b, t)),
		static_cast<std::uint8_t>(Lerp(lhs.a, rhs.a, t))
	};
}

template <typename U, type_traits::floating_point<U> = true>
[[nodiscard]] inline Color Lerp(const Color& lhs, const Color& rhs, U t_r, U t_g, U t_b, U t_a) {
	return Color{
		static_cast<std::uint8_t>(Lerp(lhs.r, rhs.r, t_r)),
		static_cast<std::uint8_t>(Lerp(lhs.g, rhs.g, t_g)),
		static_cast<std::uint8_t>(Lerp(lhs.b, rhs.b, t_b)),
		static_cast<std::uint8_t>(Lerp(lhs.a, rhs.a, t_a))
	};
}

namespace color {

inline constexpr Color TRANSPARENT   {   0,   0,   0,   0 };
inline constexpr Color BLACK         {   0,   0,   0, 255 };
inline constexpr Color WHITE         { 255, 255, 255, 255 };
inline constexpr Color RED           { 255,   0,   0, 255 };
inline constexpr Color DARK_RED      { 128,   0,   0, 255 };
inline constexpr Color BROWN         { 128,  64,  32, 255 };
inline constexpr Color DARK_BROWN    {  64,  32,  16, 255 };
inline constexpr Color ORANGE        { 255, 165,   0, 255 };
inline constexpr Color YELLOW        { 255, 255,   0, 255 };
inline constexpr Color GOLD          { 255, 215,   0, 255 };
inline constexpr Color GREEN         {   0, 128,   0, 255 };
inline constexpr Color LIME          {   0, 255,   0, 255 };
inline constexpr Color DARK_GREEN    {   0, 100,   0, 255 };
inline constexpr Color BLUE          {   0,   0, 255, 255 };
inline constexpr Color DARK_BLUE     {   0,   0, 128, 255 };
inline constexpr Color CYAN          {   0, 255, 255, 255 };
inline constexpr Color TEAL          {   0, 128, 128, 255 };
inline constexpr Color MAGENTA       { 255,   0, 255, 255 };
inline constexpr Color PURPLE        { 128,   0, 128, 255 };
inline constexpr Color PINK          { 255, 192, 203, 255 };
inline constexpr Color LIGHT_PINK    { 255, 128, 255, 255 };
inline constexpr Color GREY          { 128, 128, 128, 255 };
inline constexpr Color DARK_GREY     {  64,  64,  64, 255 };
inline constexpr Color LIGHT_GREY    {  83,  83,  83, 255 };
inline constexpr Color SILVER        { 192, 192, 192, 255 };

} // namespace color

} // namespace ptgn
