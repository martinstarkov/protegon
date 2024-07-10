#pragma once

#include <array>
#include <cstdint>

#include "protegon/type_traits.h"

namespace ptgn {

// Vertex Types

namespace glsl {

using float_ = std::array<float, 1>;
using vec2	 = std::array<float, 2>;
using vec3	 = std::array<float, 3>;
using vec4	 = std::array<float, 4>;

using double_ = std::array<double, 1>;
using dvec2	  = std::array<double, 2>;
using dvec3	  = std::array<double, 3>;
using dvec4	  = std::array<double, 4>;

using bool_ = std::array<bool, 1>;
using bvec2 = std::array<bool, 2>;
using bvec3 = std::array<bool, 3>;
using bvec4 = std::array<bool, 4>;

using int_	= std::array<int, 1>;
using ivec2 = std::array<int, 2>;
using ivec3 = std::array<int, 3>;
using ivec4 = std::array<int, 4>;

using uint_ = std::array<unsigned int, 1>;
using uvec2 = std::array<unsigned int, 2>;
using uvec3 = std::array<unsigned int, 3>;
using uvec4 = std::array<unsigned int, 4>;

} // namespace glsl

enum class PrimitiveMode : std::uint32_t {
	Points		  = 0x0000, // GL_POINTS
	Lines		  = 0x0001, // GL_LINES
	LineLoop	  = 0x0002, // GL_LINE_LOOP
	LineStrip	  = 0x0003, // GL_LINE_STRIP
	Triangles	  = 0x0004, // GL_TRIANGLES
	TriangleStrip = 0x0005, // GL_TRIANGLE_STRIP
	TriangleFan	  = 0x0006, // GL_TRIANGLE_FAN
	Quads		  = 0x0007, // GL_QUADS
	QuadStrip	  = 0x0008, // GL_QUAD_STRIP
	Polygon		  = 0x0009	// GL_POLYGON
};

namespace impl {

template <typename T>
inline constexpr bool is_vertex_data_type{ type_traits::is_one_of_v<
	T, glsl::float_, glsl::vec2, glsl::vec3, glsl::vec4, glsl::double_, glsl::dvec2, glsl::dvec3,
	glsl::dvec4, glsl::bool_, glsl::bvec2, glsl::bvec3, glsl::bvec4, glsl::int_, glsl::ivec2,
	glsl::ivec3, glsl::ivec4, glsl::uint_, glsl::uvec2, glsl::uvec3, glsl::uvec4> };

enum class GLSLType : std::uint32_t {
	None		  = 0,
	Byte		  = 0x1400, // GL_BYTE
	UnsignedByte  = 0x1401, // GL_UNSIGNED_BYTE
	Short		  = 0x1402, // GL_SHORT
	UnsignedShort = 0x1403, // GL_UNSIGNED_SHORT
	Int			  = 0x1404, // GL_INT
	UnsignedInt	  = 0x1405, // GL_UNSIGNED_INT
	Float		  = 0x1406, // GL_FLOAT
	Double		  = 0x140A, // GL_DOUBLE
};

template <typename T>
[[nodiscard]] GLSLType GetType() {
	static_assert(
		type_traits::is_one_of_v<
			T, float, double, std::int32_t, std::uint32_t, std::int16_t, std::uint16_t, std::int8_t,
			std::uint8_t, bool>,
		"Cannot retrieve type which is not supported by OpenGL"
	);

	if constexpr (std::is_same_v<T, float>) {
		return GLSLType::Float;
	} else if constexpr (std::is_same_v<T, double>) {
		return GLSLType::Double;
	} else if constexpr (std::is_same_v<T, std::int32_t>) {
		return GLSLType::Int;
	} else if constexpr (std::is_same_v<T, std::uint32_t>) {
		return GLSLType::UnsignedInt;
	} else if constexpr (std::is_same_v<T, std::int16_t>) {
		return GLSLType::Short;
	} else if constexpr (std::is_same_v<T, std::uint16_t>) {
		return GLSLType::UnsignedShort;
	} else if constexpr (std::is_same_v<T, std::int8_t> || std::is_same_v<T, bool>) {
		return GLSLType::Byte;
	} else if constexpr (std::is_same_v<T, std::uint8_t>) {
		return GLSLType::UnsignedByte;
	}
}

} // namespace impl

} // namespace ptgn