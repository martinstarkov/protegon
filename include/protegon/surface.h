#pragma once

#include <vector>

#include "protegon/color.h"
#include "protegon/file.h"
#include "protegon/vector2.h"
#include "utility/handle.h"

struct SDL_Surface;

namespace ptgn {

class Text;

enum class ImageFormat {
	Unknown	 = 0,		  // SDL_PIXELFORMAT_UNKNOWN
	RGB888	 = 370546692, // SDL_PIXELFORMAT_RGB888;
	RGBA8888 = 373694468, // SDL_PIXELFORMAT_RGBA8888;
	BGRA8888 = 377888772, // SDL_PIXELFORMAT_BGRA8888;
	BGR888	 = 374740996, // SDL_PIXELFORMAT_BGR888;
};

namespace impl {

struct SurfaceInstance {
	SurfaceInstance()  = default;
	~SurfaceInstance() = default;
	ImageFormat format_{ ImageFormat::Unknown };
	std::vector<Color> data_;
	V2_int size_;
};

} // namespace impl

class Surface : public Handle<impl::SurfaceInstance> {
public:
	Surface() = default;
	Surface(const path& image_path);

	void FlipVertically();

	void ForEachPixel(std::function<void(const V2_int&, const Color&)> function);

	[[nodiscard]] V2_int GetSize() const;
	[[nodiscard]] const std::vector<Color>& GetData() const;
	[[nodiscard]] ImageFormat GetImageFormat() const;

private:
	friend class Text;

	Surface(
		const std::shared_ptr<SDL_Surface>& surface, ImageFormat format = ImageFormat::RGBA8888
	);
};

} // namespace ptgn