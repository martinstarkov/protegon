#include "renderer/image/image_utils.h"

#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/image/surface.h"

namespace ptgn {

Color GetPixel(const path& texture_filepath, V2_int coordinate) {
	impl::Surface s{ texture_filepath };
	return s.GetPixel(coordinate);
}

V2_int ForEachPixel(
	const path& texture_filepath, const std::function<void(V2_int, Color)>& function
) {
	impl::Surface s{ texture_filepath };
	s.ForEachPixel(function);
	return s.size;
}

} // namespace ptgn