#pragma once

#include "core/util/file.h"
#include "math/vector2.h"
#include "renderer/api/color.h"

namespace ptgn {

// @param coordinate Pixel coordinate from [0, size).
// @return Color value of the given pixel.
[[nodiscard]] Color GetPixel(const path& texture_filepath, V2_int coordinate);

// @param callback Function to be called for each pixel.
// @return The pixel size of the looped texture.
V2_int ForEachPixel(
	const path& texture_filepath, const std::function<void(V2_int, Color)>& function
);

} // namespace ptgn