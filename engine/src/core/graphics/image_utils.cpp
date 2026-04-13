#include "renderer/image/image_utils.h"

#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/image/surface.h"
#include "core/graphics/color.h"

namespace ptgn {

Color GetPixel(const path& texture_filepath, V2_int coordinate) {
	impl::Surface s{ texture_filepath };
	return s.GetPixel(coordinate);
}

} // namespace ptgn