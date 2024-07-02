#pragma once

#include <vector>

#include "file.h"
#include "handle.h"
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
	std::vector<std::uint8_t> data_;
	V2_int size_;
	std::int32_t pitch_{ 0 };
	std::int32_t bytes_per_pixel_{ 0 };
};

} // namespace impl

class Surface : public Handle<impl::SurfaceInstance> {
public:
	Surface() = default;
	Surface(const path& image_path, ImageFormat format = ImageFormat::RGBA8888);

	void FlipVertically();

	[[nodiscard]] V2_int GetSize() const;
	[[nodiscard]] std::int32_t GetBytesPerPixel() const;
	[[nodiscard]] std::int32_t GetPitch() const;
	[[nodiscard]] const std::vector<std::uint8_t>& GetData() const;
	[[nodiscard]] ImageFormat GetImageFormat() const;
};

} // namespace ptgn