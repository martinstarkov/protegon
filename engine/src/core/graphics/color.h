#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/log.h"
#include "core/assert.h"
#include "core/math/math_utils.h"
#include "core/math/rng.h"
#include "core/math/vector4.h"
#include "core/util/concepts.h"
#include "core/util/hash.h"
#include "serialization/json/fwd.h"
#include "serialization/json/json.h"

namespace ptgn {

enum class ColorPacking {
	RGBA,
	ABGR,
};

/// @brief 8-bit RGBA color.
/// Default: Transparent.
struct Color {
	std::uint8_t r{ 0 };
	std::uint8_t g{ 0 };
	std::uint8_t b{ 0 };
	std::uint8_t a{ 0 };

	/// @return Pointer to the first color component (r) of the contiguous RGBA byte data.
	constexpr std::uint8_t* Data() noexcept {
		static_assert(std::is_standard_layout_v<Color>);
		return &r;
	}

	constexpr const std::uint8_t* Data() const noexcept {
		static_assert(std::is_standard_layout_v<Color>);
		return &r;
	}

	/// @brief Default: Transparent.
	constexpr Color() = default;

	constexpr Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) :
		r{ red }, g{ green }, b{ blue }, a{ alpha } {}

	/// @brief Constructs from normalized RGBA values [r, g, b, a].
	/// @param color Components expected in range [0.0, 1.0].
	explicit constexpr Color(std::array<float, 4> color) :
		Color{ V4_float{ color[0], color[1], color[2], color[3] } } {}

	explicit constexpr Color(std::array<std::uint8_t, 4> color) :
		Color{ color[0], color[1], color[2], color[3] } {}

	/// @brief Constructs from normalized RGBA vector (x == r, y == g, z == b, w == a).
	/// @param color Components expected in range [0.0, 1.0].
	explicit constexpr Color(V4_float color) :
		r{ static_cast<std::uint8_t>(color.x * 255.0f) },
		g{ static_cast<std::uint8_t>(color.y * 255.0f) },
		b{ static_cast<std::uint8_t>(color.z * 255.0f) },
		a{ static_cast<std::uint8_t>(color.w * 255.0f) } {
		PTGN_ASSERT(color.IsNormalized(), "Color must be normalized");
	}

	/// @brief Constructs from packed RGBA value in the form 0xRRGGBBAA.
	explicit constexpr Color(std::uint32_t rgba) :
		r{ static_cast<std::uint8_t>((rgba >> 24) & 255) },
		g{ static_cast<std::uint8_t>((rgba >> 16) & 255) },
		b{ static_cast<std::uint8_t>((rgba >> 8) & 255) },
		a{ static_cast<std::uint8_t>(rgba & 255) } {}

	constexpr std::uint32_t ToUint32(ColorPacking packing = ColorPacking::RGBA) const {
		switch (packing) {
			case ColorPacking::RGBA:
				return (static_cast<std::uint32_t>(r) << 24u) |
					(static_cast<std::uint32_t>(g) << 16u) |
					(static_cast<std::uint32_t>(b) << 8u) |
					static_cast<std::uint32_t>(a);
			case ColorPacking::ABGR:
				return (static_cast<std::uint32_t>(a) << 24u) |
					(static_cast<std::uint32_t>(b) << 16u) |
					(static_cast<std::uint32_t>(g) << 8u) |
					static_cast<std::uint32_t>(r);
			default: PTGN_ERROR("Unknown ColorPacking: ", std::to_underlying(packing));
		}
	}

	/// @param alpha Value of transparency to set for the color.
	/// @return A copy of the color with the modified alpha channel.
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

	/// @return Color values normalized in range [0.0, 1.0].
	[[nodiscard]] constexpr V4_float Normalized() const {
		return { static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
				 static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f };
	}

	[[nodiscard]] constexpr static Color Multiply(Color lhs, Color rhs) {
		return Color{ lhs.Normalized() * rhs.Normalized() };
	}

	constexpr explicit operator V4_float() const {
		return Normalized();
	}

	constexpr explicit operator std::array<float, 4>() const {
		auto n{ Normalized() };
		return { n.x, n.y, n.z, n.w };
	}

	constexpr explicit operator std::array<std::uint8_t, 4>() const {
		return { r, g, b, a };
	}

	/// @brief Generates a random fully opaque color.
	[[nodiscard]] static Color RandomOpaque() {
		static RNG<int> rng{ 0, 255 };
		return { static_cast<std::uint8_t>(rng()), static_cast<std::uint8_t>(rng()),
				 static_cast<std::uint8_t>(rng()), 255 };
	}

	/// @brief Generates a random color including a random alpha.
	[[nodiscard]] static Color RandomTransparent() {
		static RNG<int> rng{ 0, 255 };
		return { static_cast<std::uint8_t>(rng()), static_cast<std::uint8_t>(rng()),
				 static_cast<std::uint8_t>(rng()), static_cast<std::uint8_t>(rng()) };
	}

	/// @return True if color is fully transparent.
	constexpr bool IsTransparent() const {
		return a == 0;
	}

	/// @return True if color is fully opaque.
	constexpr bool IsOpaque() const {
		return a == 255;
	}

	constexpr bool operator==(const Color&) const = default;

	friend void to_json(json& j, const Color& color) {
		j = json::array({ color.r, color.g, color.b, color.a });
	}

	friend void from_json(const json& j, Color& color) {
		PTGN_ASSERT(j.is_array(), "Deserializing a Color from json requires an array");
		PTGN_ASSERT(
			j.size() == 4, "Deserializing a Color from json requires an array with four elements"
		);

		PTGN_ASSERT(j[0].is_number_unsigned(), "Color array elements must be unsigned integers");
		PTGN_ASSERT(j[1].is_number_unsigned(), "Color array elements must be unsigned integers");
		PTGN_ASSERT(j[2].is_number_unsigned(), "Color array elements must be unsigned integers");
		PTGN_ASSERT(j[3].is_number_unsigned(), "Color array elements must be unsigned integers");

		PTGN_ASSERT(j[0] >= 0 && j[0] <= 255, "Color value outside of range [0, 255]");
		PTGN_ASSERT(j[1] >= 0 && j[1] <= 255, "Color value outside of range [0, 255]");
		PTGN_ASSERT(j[2] >= 0 && j[2] <= 255, "Color value outside of range [0, 255]");
		PTGN_ASSERT(j[3] >= 0 && j[3] <= 255, "Color value outside of range [0, 255]");

		color.r = j[0];
		color.g = j[1];
		color.b = j[2];
		color.a = j[3];
	}

	friend std::ostream& operator<<(std::ostream& os, Color color) {
		os << "[";
		os << static_cast<int>(color.r) << ", ";
		os << static_cast<int>(color.g) << ", ";
		os << static_cast<int>(color.b) << ", ";
		os << static_cast<int>(color.a);
		os << "]";
		return os;
	}
};

/// @brief Linearly interpolates between two colors.
/// @param t Interpolation factor in range [0.0, 1.0].
[[nodiscard]] constexpr Color Lerp(Color lhs, Color rhs, float t) {
	return Color{ static_cast<std::uint8_t>(Lerp(lhs.r, rhs.r, t)),
				  static_cast<std::uint8_t>(Lerp(lhs.g, rhs.g, t)),
				  static_cast<std::uint8_t>(Lerp(lhs.b, rhs.b, t)),
				  static_cast<std::uint8_t>(Lerp(lhs.a, rhs.a, t)) };
}

/// @brief Linearly interpolates between two colors (per channel).
/// @param t Separate RGBA interpolation factors in range [0.0, 1.0]
[[nodiscard]] constexpr Color Lerp(Color lhs, Color rhs, V4_float t) {
	return Color{ static_cast<std::uint8_t>(Lerp(lhs.r, rhs.r, t.x)),
				  static_cast<std::uint8_t>(Lerp(lhs.g, rhs.g, t.y)),
				  static_cast<std::uint8_t>(Lerp(lhs.b, rhs.b, t.z)),
				  static_cast<std::uint8_t>(Lerp(lhs.a, rhs.a, t.w)) };
}

struct RegisteredColor {
	std::string key{};
	Color value{};

	bool operator==(const RegisteredColor&) const = default;
};

namespace impl {

inline std::vector<RegisteredColor>& MutableColorRegistry() {
	static std::vector<RegisteredColor> registry;
	return registry;
}

} // namespace impl

/// @brief Registers a named color. Keys must be unique.
/// Re-registering the same key/value pair is harmless, which keeps header-defined registrations
/// safe across translation units.
inline bool RegisterColor(std::string key, Color value) {
	PTGN_ASSERT(!key.empty(), "Registered color key cannot be empty");

	auto& registry{ impl::MutableColorRegistry() };
	const auto it{ std::find_if(
		registry.begin(),
		registry.end(),
		[&key](const RegisteredColor& color) {
			return color.key == key;
		}
	) };

	if (it != registry.end()) {
		PTGN_ASSERT(
			it->value == value,
			"Registered color key already exists with a different value: ",
			key
		);
		return false;
	}

	registry.push_back(RegisteredColor{
		.key = std::move(key),
		.value = value,
	});
	return true;
}

/// @return Every registered engine/user color in registration order.
[[nodiscard]] inline const std::vector<RegisteredColor>& GetRegisteredColors() {
	return impl::MutableColorRegistry();
}

[[nodiscard]] inline const RegisteredColor* FindRegisteredColor(std::string_view key) {
	const auto& registry{ GetRegisteredColors() };
	const auto it{ std::find_if(
		registry.begin(),
		registry.end(),
		[key](const RegisteredColor& color) {
			return color.key == key;
		}
	) };
	return it == registry.end() ? nullptr : std::addressof(*it);
}

[[nodiscard]] inline const RegisteredColor* FindRegisteredColor(Color value) {
	const auto& registry{ GetRegisteredColors() };
	const auto it{ std::find_if(
		registry.begin(),
		registry.end(),
		[value](const RegisteredColor& color) {
			return color.value == value;
		}
	) };
	return it == registry.end() ? nullptr : std::addressof(*it);
}

} // namespace ptgn

/// @brief Declares ptgn::color::Name as an inline constexpr Color and adds it to the global color
/// registry under Key. Invoke this macro at global namespace scope.
#define PTGN_REGISTER_COLOR(Name, Key, Red, Green, Blue, Alpha)                         \
	namespace ptgn::color {                                                              \
	inline constexpr ::ptgn::Color Name{                                                  \
		static_cast<std::uint8_t>(Red),                                                    \
		static_cast<std::uint8_t>(Green),                                                  \
		static_cast<std::uint8_t>(Blue),                                                   \
		static_cast<std::uint8_t>(Alpha)                                                   \
	};                                                                                    \
	}                                                                                     \
	namespace ptgn::impl {                                                               \
	[[maybe_unused]] inline const bool Name##_registered_color{                                            \
		::ptgn::RegisterColor((Key), ::ptgn::color::Name)                                  \
	};                                                                                    \
	}

PTGN_REGISTER_COLOR(Transparent, "Transparent", 0, 0, 0, 0);
PTGN_REGISTER_COLOR(Black, "Black", 0, 0, 0, 255);
PTGN_REGISTER_COLOR(White, "White", 255, 255, 255, 255);

PTGN_REGISTER_COLOR(Red, "Red", 255, 0, 0, 255);
PTGN_REGISTER_COLOR(LightRed, "Light Red", 255, 128, 128, 255);
PTGN_REGISTER_COLOR(DarkRed, "Dark Red", 128, 0, 0, 255);
PTGN_REGISTER_COLOR(BrightRed, "Bright Red", 255, 69, 0, 255);
PTGN_REGISTER_COLOR(DeepRed, "Deep Red", 178, 34, 34, 255);

PTGN_REGISTER_COLOR(Brown, "Brown", 150, 75, 0, 255);
PTGN_REGISTER_COLOR(LightBrown, "Light Brown", 210, 180, 140, 255);
PTGN_REGISTER_COLOR(DarkBrown, "Dark Brown", 101, 67, 33, 255);

PTGN_REGISTER_COLOR(Orange, "Orange", 255, 165, 0, 255);
PTGN_REGISTER_COLOR(LightOrange, "Light Orange", 255, 215, 128, 255);
PTGN_REGISTER_COLOR(DarkOrange, "Dark Orange", 204, 102, 0, 255);

PTGN_REGISTER_COLOR(Yellow, "Yellow", 255, 255, 0, 255);
PTGN_REGISTER_COLOR(LightYellow, "Light Yellow", 255, 255, 128, 255);
PTGN_REGISTER_COLOR(DarkYellow, "Dark Yellow", 204, 204, 0, 255);
PTGN_REGISTER_COLOR(BrightYellow, "Bright Yellow", 255, 255, 102, 255);
PTGN_REGISTER_COLOR(Gold, "Gold", 255, 215, 0, 255);
PTGN_REGISTER_COLOR(LightGold, "Light Gold", 255, 235, 153, 255);
PTGN_REGISTER_COLOR(DarkGold, "Dark Gold", 184, 134, 11, 255);

PTGN_REGISTER_COLOR(Green, "Green", 0, 255, 0, 255);
PTGN_REGISTER_COLOR(LightGreen, "Light Green", 144, 238, 144, 255);
PTGN_REGISTER_COLOR(DarkGreen, "Dark Green", 0, 100, 0, 255);
PTGN_REGISTER_COLOR(BrightGreen, "Bright Green", 0, 255, 102, 255);
PTGN_REGISTER_COLOR(LimeGreen, "Lime Green", 191, 255, 0, 255);

PTGN_REGISTER_COLOR(Blue, "Blue", 0, 0, 255, 255);
PTGN_REGISTER_COLOR(LightBlue, "Light Blue", 173, 216, 230, 255);
PTGN_REGISTER_COLOR(DarkBlue, "Dark Blue", 0, 0, 128, 255);
PTGN_REGISTER_COLOR(SkyBlue, "Sky Blue", 135, 206, 235, 255);
PTGN_REGISTER_COLOR(DeepBlue, "Deep Blue", 0, 70, 128, 255);

PTGN_REGISTER_COLOR(Cyan, "Cyan", 0, 255, 255, 255);
PTGN_REGISTER_COLOR(LightCyan, "Light Cyan", 224, 255, 255, 255);
PTGN_REGISTER_COLOR(DarkCyan, "Dark Cyan", 0, 139, 139, 255);
PTGN_REGISTER_COLOR(Teal, "Teal", 0, 128, 128, 255);
PTGN_REGISTER_COLOR(LightTeal, "Light Teal", 128, 255, 212, 255);
PTGN_REGISTER_COLOR(DarkTeal, "Dark Teal", 0, 80, 80, 255);

PTGN_REGISTER_COLOR(Magenta, "Magenta", 255, 0, 255, 255);
PTGN_REGISTER_COLOR(LightMagenta, "Light Magenta", 255, 105, 180, 255);
PTGN_REGISTER_COLOR(DarkMagenta, "Dark Magenta", 139, 0, 139, 255);
PTGN_REGISTER_COLOR(Purple, "Purple", 128, 0, 128, 255);
PTGN_REGISTER_COLOR(LightPurple, "Light Purple", 178, 102, 255, 255);
PTGN_REGISTER_COLOR(DarkPurple, "Dark Purple", 75, 0, 130, 255);

PTGN_REGISTER_COLOR(Pink, "Pink", 255, 192, 203, 255);
PTGN_REGISTER_COLOR(LightPink, "Light Pink", 255, 182, 193, 255);
PTGN_REGISTER_COLOR(DarkPink, "Dark Pink", 197, 137, 123, 255);
PTGN_REGISTER_COLOR(BrightPink, "Bright Pink", 255, 0, 127, 255);

PTGN_REGISTER_COLOR(Gray, "Gray", 128, 128, 128, 255);
PTGN_REGISTER_COLOR(LightGray, "Light Gray", 192, 192, 192, 255);
PTGN_REGISTER_COLOR(DarkGray, "Dark Gray", 64, 64, 64, 255);

PTGN_REGISTER_COLOR(Beige, "Beige", 245, 245, 220, 255);
PTGN_REGISTER_COLOR(IvoryWhite, "Ivory White", 255, 240, 240, 255);
PTGN_REGISTER_COLOR(KhakiTan, "Khaki Tan", 240, 230, 140, 255);

template <>
struct std::hash<ptgn::Color> {
	std::size_t operator()(const ptgn::Color& color) const {
		return ptgn::Hash(color.r, color.g, color.b, color.a);
	}
};