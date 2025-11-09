#pragma once

#include <concepts>
#include <cstdint>
#include <ostream>
#include <type_traits>

#include "core/util/concepts.h"
#include "debug/runtime/assert.h"
#include "math/vector4.h"
#include "serialization/json/fwd.h"

// TODO: Stop exposing assert.h

struct SDL_Color;

namespace ptgn {

struct Color {
	std::uint8_t r{ 0 };
	std::uint8_t g{ 0 };
	std::uint8_t b{ 0 };
	std::uint8_t a{ 0 };

	constexpr std::uint8_t* Data() noexcept {
		static_assert(std::is_standard_layout_v<Color>);
		return &r;
	}

	constexpr const std::uint8_t* Data() const noexcept {
		static_assert(std::is_standard_layout_v<Color>);
		return &r;
	}

	// Default color is black.
	constexpr Color() = default;

	constexpr Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) :
		r{ red }, g{ green }, b{ blue }, a{ alpha } {}

	explicit constexpr Color(const std::array<float, 4>& color) :
		Color{ V4_float{ color[0], color[1], color[2], color[3] } } {}

	explicit constexpr Color(const std::array<std::uint8_t, 4>& color) :
		Color{ color[0], color[1], color[2], color[3] } {}

	explicit constexpr Color(const V4_float& color) :
		r{ static_cast<std::uint8_t>(color.x * 255.0f) },
		g{ static_cast<std::uint8_t>(color.y * 255.0f) },
		b{ static_cast<std::uint8_t>(color.z * 255.0f) },
		a{ static_cast<std::uint8_t>(color.w * 255.0f) } {
		PTGN_ASSERT(color.IsNormalized(), "Color must be normalized");
	}

	// @param alpha Value of transparency to set for the color.
	// @return A copy of the color with the alpha value changed.
	template <Arithmetic T>
	[[nodiscard]] constexpr Color WithAlpha(T alpha) const {
		if constexpr (std::is_floating_point_v<T>) {
			PTGN_ASSERT(alpha >= 0.0 && alpha <= 1.0, "Alpha out of range");
			Color c{ *this };
			c.a = static_cast<std::uint8_t>(255.0f * alpha);
			return c;
		} else {
			PTGN_ASSERT(alpha >= 0 && alpha <= 255, "Alpha out of range");
			Color c{ *this };
			c.a = static_cast<std::uint8_t>(alpha);
			return c;
		}
	}

	// @return Color values normalized to [0, 1] range.
	[[nodiscard]] constexpr V4_float Normalized() const {
		return { static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
				 static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f };
	}

	explicit operator V4_float() const noexcept {
		return Normalized();
	}

	explicit operator std::array<float, 4>() const noexcept {
		auto n{ Normalized() };
		return { n.x, n.y, n.z, n.w };
	}

	explicit operator std::array<std::uint8_t, 4>() const noexcept {
		return { r, g, b, a };
	}

	[[nodiscard]] static Color RandomOpaque();
	[[nodiscard]] static Color RandomTransparent();

	bool operator==(const Color&) const = default;

	friend void to_json(json& j, const Color& color);

	friend void from_json(const json& j, Color& color);

	friend std::ostream& operator<<(std::ostream& os, const Color& color) {
		os << "[";
		os << static_cast<int>(color.r) << ", ";
		os << static_cast<int>(color.g) << ", ";
		os << static_cast<int>(color.b) << ", ";
		os << static_cast<int>(color.a);
		os << "]";
		return os;
	}
};

template <std::floating_point U>
[[nodiscard]] inline Color Lerp(const Color& lhs, const Color& rhs, U t) {
	return Color{ static_cast<std::uint8_t>(Lerp(lhs.r, rhs.r, t)),
				  static_cast<std::uint8_t>(Lerp(lhs.g, rhs.g, t)),
				  static_cast<std::uint8_t>(Lerp(lhs.b, rhs.b, t)),
				  static_cast<std::uint8_t>(Lerp(lhs.a, rhs.a, t)) };
}

template <std::floating_point U>
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
inline constexpr Color LightRed{ 255, 128, 128, 255 };
inline constexpr Color DarkRed{ 128, 0, 0, 255 };
inline constexpr Color BrightRed{ 255, 69, 0, 255 };
inline constexpr Color DeepRed{ 178, 34, 34, 255 };

inline constexpr Color Brown{ 165, 42, 42, 255 };
inline constexpr Color LightBrown{ 210, 180, 140, 255 };
inline constexpr Color DarkBrown{ 101, 67, 33, 255 };

inline constexpr Color Orange{ 255, 165, 0, 255 };
inline constexpr Color LightOrange{ 255, 215, 128, 255 };
inline constexpr Color DarkOrange{ 204, 102, 0, 255 };

inline constexpr Color Yellow{ 255, 255, 0, 255 };
inline constexpr Color LightYellow{ 255, 255, 128, 255 };
inline constexpr Color DarkYellow{ 204, 204, 0, 255 };
inline constexpr Color BrightYellow{ 255, 255, 102, 255 };
inline constexpr Color Gold{ 255, 215, 0, 255 };
inline constexpr Color LightGold{ 255, 235, 153, 255 };
inline constexpr Color DarkGold{ 184, 134, 11, 255 };

inline constexpr Color Green{ 0, 255, 0, 255 };
inline constexpr Color LightGreen{ 144, 238, 144, 255 };
inline constexpr Color DarkGreen{ 0, 100, 0, 255 };
inline constexpr Color BrightGreen{ 0, 255, 102, 255 };
inline constexpr Color LimeGreen{ 191, 255, 0, 255 };

inline constexpr Color Blue{ 0, 0, 255, 255 };
inline constexpr Color LightBlue{ 173, 216, 230, 255 };
inline constexpr Color DarkBlue{ 0, 0, 128, 255 };
inline constexpr Color SkyBlue{ 135, 206, 235, 255 };
inline constexpr Color DeepBlue{ 0, 70, 128, 255 };

inline constexpr Color Cyan{ 0, 255, 255, 255 };
inline constexpr Color LightCyan{ 224, 255, 255, 255 };
inline constexpr Color DarkCyan{ 0, 139, 139, 255 };
inline constexpr Color Teal{ 0, 128, 128, 255 };
inline constexpr Color LightTeal{ 128, 255, 212, 255 };
inline constexpr Color DarkTeal{ 0, 80, 80, 255 };

inline constexpr Color Magenta{ 255, 0, 255, 255 };
inline constexpr Color LightMagenta{ 255, 105, 180, 255 };
inline constexpr Color DarkMagenta{ 139, 0, 139, 255 };
inline constexpr Color Purple{ 128, 0, 128, 255 };
inline constexpr Color LightPurple{ 178, 102, 255, 255 };
inline constexpr Color DarkPurple{ 75, 0, 130, 255 };

inline constexpr Color Pink{ 255, 192, 203, 255 };
inline constexpr Color LightPink{ 255, 182, 193, 255 };
inline constexpr Color DarkPink{ 197, 137, 123, 255 };
inline constexpr Color BrightPink{ 255, 0, 127, 255 };

inline constexpr Color Gray{ 128, 128, 128, 255 };
inline constexpr Color LightGray{ 192, 192, 192, 255 };
inline constexpr Color DarkGray{ 64, 64, 64, 255 };

inline constexpr Color Beige{ 245, 245, 220, 255 };
inline constexpr Color IvoryWhite{ 255, 240, 240, 255 };
inline constexpr Color KhakiTan{ 240, 230, 140, 255 };

} // namespace color

} // namespace ptgn