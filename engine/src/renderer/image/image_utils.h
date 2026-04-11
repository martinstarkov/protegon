#pragma once

#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/file.h"
#include "renderer/image/surface.h"
#include "renderer/primitives/color.h"

namespace ptgn {

/// @param coordinate Pixel coordinate from [0, size).
/// @return Color value of the given pixel.
Color GetPixel(const path& texture_filepath, V2_int coordinate);

/// @brief Calls the given function for each pixel in the texture in row-major order.
/// @param function The function must be callable as void(V2_int, Color).
/// @return The pixel size of the looped texture.
template <InvocableR<void, V2_int, Color> F>
V2_int ForEachPixel(const path& texture_filepath, F&& func) {
	impl::Surface s{ texture_filepath };
	s.ForEachPixel(std::forward<F>(func));
	return s.GetSize();
}

} // namespace ptgn