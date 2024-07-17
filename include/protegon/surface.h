#pragma once

#include <vector>

#include "file.h"
#include "utility/handle.h"
#include "vector2.h"

namespace ptgn {

enum class ImageFormat {
	Unknown	 = 0,		  // SDL_PIXELFORMAT_UNKNOWN
	RGB888	 = 374740996, // SDL_PIXELFORMAT_BGR888
	RGBA8888 = 373694468, // SDL_PIXELFORMAT_RGBA8888
	RGBA32	 = 376840196  // SDL_PIXELFORMAT_RGBA32
};

namespace impl {

struct SurfaceInstance {
	SurfaceInstance(const path& image_path, ImageFormat format);
	ImageFormat format_{ ImageFormat::Unknown };
	std::vector<Color> data_;
	V2_int size_;
};

} // namespace impl

class Surface : public Handle<impl::SurfaceInstance> {
public:
	Surface() = default;
	Surface(const path& image_path, ImageFormat format = ImageFormat::RGBA8888);

	void FlipVertically();

	void ForEachPixel(std::function<void(const V2_int&, const Color&)> function);

	[[nodiscard]] V2_int GetSize() const;
	[[nodiscard]] const std::vector<Color>& GetData() const;
	[[nodiscard]] ImageFormat GetImageFormat() const;
};

} // namespace ptgn