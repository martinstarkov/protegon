#include "renderer/primitives/color.h"

#include <array>
#include <cstdint>

#include "core/assert.h"
#include "core/math/math_utils.h"
#include "core/math/rng.h"
#include "core/math/vector4.h"
#include "serialization/json/json.h"

namespace ptgn {

Color::operator V4_float() const {
	return Normalized();
}

Color::operator std::array<float, 4>() const {
	auto n{ Normalized() };
	return { n.x, n.y, n.z, n.w };
}

Color::operator std::array<std::uint8_t, 4>() const {
	return { r, g, b, a };
}

Color Color::RandomOpaque() {
	static RNG<int> rng{ 0, 255 };
	return { static_cast<std::uint8_t>(rng()), static_cast<std::uint8_t>(rng()),
			 static_cast<std::uint8_t>(rng()), 255 };
}

Color Color::RandomTransparent() {
	static RNG<int> rng{ 0, 255 };
	return { static_cast<std::uint8_t>(rng()), static_cast<std::uint8_t>(rng()),
			 static_cast<std::uint8_t>(rng()), static_cast<std::uint8_t>(rng()) };
}

void to_json(json& j, const Color& color) {
	j = json::array({ color.r, color.g, color.b, color.a });
}

void from_json(const json& j, Color& color) {
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

Color Lerp(Color lhs, Color rhs, float t) {
	return Color{ static_cast<std::uint8_t>(Lerp(lhs.r, rhs.r, t)),
				  static_cast<std::uint8_t>(Lerp(lhs.g, rhs.g, t)),
				  static_cast<std::uint8_t>(Lerp(lhs.b, rhs.b, t)),
				  static_cast<std::uint8_t>(Lerp(lhs.a, rhs.a, t)) };
}

Color Lerp(Color lhs, Color rhs, V4_float t) {
	return Color{ static_cast<std::uint8_t>(Lerp(lhs.r, rhs.r, t.x)),
				  static_cast<std::uint8_t>(Lerp(lhs.g, rhs.g, t.y)),
				  static_cast<std::uint8_t>(Lerp(lhs.b, rhs.b, t.z)),
				  static_cast<std::uint8_t>(Lerp(lhs.a, rhs.a, t.w)) };
}

} // namespace ptgn