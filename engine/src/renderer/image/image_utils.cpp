#include "renderer/image/image_utils.h"

#include "renderer/image/surface.h"

namespace ptgn {

Color GetPixel(const path& texture_filepath, const V2_int& coordinate) {
	impl::Surface s{ texture_filepath };
	return s.GetPixel(coordinate);
}

V2_int ForEachPixel(
	const path& texture_filepath, const std::function<void(const V2_int&, const Color&)>& function
) {
	impl::Surface s{ texture_filepath };
	s.ForEachPixel(function);
	return s.size;
}

} // namespace ptgn