#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "math/vector2.h"
#include "renderer/flip.h"

namespace ptgn::impl {

// TODO: Move these somewhere.

[[nodiscard]] static std::array<V2_float, 4> GetTextureCoordinates(
	const V2_float& source_position, V2_float source_size, const V2_float& texture_size, Flip flip,
	bool offset_texels = false
);

static void FlipTextureCoordinates(std::array<V2_float, 4>& texture_coords, Flip flip);

} // namespace ptgn::impl
