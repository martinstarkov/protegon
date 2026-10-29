#pragma once

#include <utility>

#include "core/graphics/color.h"
#include "core/graphics/surface.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/file.h"

namespace ptgn {

/// @param coordinate Pixel coordinate from [0, size).
/// @return Color value of the given pixel.
Color GetPixel(const path& texture_path, V2_int coordinate) {
	impl::Surface s{ texture_path };
	return s.GetPixel(coordinate);
}

/// @brief Calls the given function for each pixel in the texture in row-major order.
/// @param function The function must be callable as void(V2_int, Color).
/// @return The pixel size of the looped texture.
template <InvocableR<void, V2_int, Color> F>
V2_int ForEachPixel(const path& texture_path, F&& func) {
	impl::Surface s{ texture_path };
	s.ForEachPixel(std::forward<F>(func));
	return s.GetSize();
}

} // namespace ptgn