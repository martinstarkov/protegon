#pragma once

#include <array>
#include <cstdint>

#include "core/util/concepts.h"

namespace ptgn::glsl {

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

using int_	= std::array<std::int32_t, 1>;
using ivec2 = std::array<std::int32_t, 2>;
using ivec3 = std::array<std::int32_t, 3>;
using ivec4 = std::array<std::int32_t, 4>;

using uint_ = std::array<std::uint32_t, 1>;
using uvec2 = std::array<std::uint32_t, 2>;
using uvec3 = std::array<std::uint32_t, 3>;
using uvec4 = std::array<std::uint32_t, 4>;

} // namespace ptgn::glsl