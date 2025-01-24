#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/font.h"
#include "utility/file.h"
#include "utility/handle.h"

struct SDL_Surface;

namespace ptgn {

class Text;

enum class TextWrapAlignment {
	Left   = 0, // TTF_WRAPPED_ALIGN_LEFT
	Center = 1, // TTF_WRAPPED_ALIGN_CENTER
	Right  = 2	// TTF_WRAPPED_ALIGN_RIGHT
};

// Format of pixels for a texture or surface.
// e.g. RGBA8888 means 8 bits per color channel (32 bits total).
enum class TextureFormat {
	Unknown	 = 0,		  // SDL_PIXELFORMAT_UNKNOWN
	RGB888	 = 370546692, // SDL_PIXELFORMAT_RGB888
	RGBA8888 = 373694468, // SDL_PIXELFORMAT_RGBA8888
	BGRA8888 = 377888772, // SDL_PIXELFORMAT_BGRA8888
	BGR888	 = 374740996, // SDL_PIXELFORMAT_BGR888
};

namespace impl {

[[nodiscard]] TextureFormat GetFormatFromSDL(std::uint32_t sdl_format);

struct SDL_SurfaceDeleter {
	void operator()(SDL_Surface* surface) const;
};

struct SurfaceInstance {
	TextureFormat format_{ TextureFormat::Unknown };
	std::vector<Color> data_;
	V2_int size_;
};

} // namespace impl

class Surface : public Handle<impl::SurfaceInstance> {
public:
	Surface() = default;

	// @param image_path Path to the image relative to the working directory.
	explicit Surface(const path& image_path);

	// Mirrors the surface vertically.
	void FlipVertically();

	// @param coordinate Pixel coordinate from [0, size).
	// @return Color value of the given pixel.
	[[nodiscard]] Color GetPixel(const V2_int& coordinate) const;

	// @param callback Function to be called for each pixel.
	void ForEachPixel(const std::function<void(const V2_int&, const Color&)>& callback);

	// @return Size of the surface.
	[[nodiscard]] V2_int GetSize() const;

	// @return Pixel format of the surface.
	[[nodiscard]] TextureFormat GetFormat() const;

	// @return The row major one dimensionalized array of pixels that makes up the surface.
	[[nodiscard]] const std::vector<Color>& GetData() const;

private:
	friend class Text;

	// Create text surface from font information.
	Surface(
		Font& font, FontStyle style, const Color& text_color, FontRenderMode mode,
		const std::string& content, std::int32_t ptsize, const Color& shading_color,
		std::uint32_t wrap_after_pixels, TextWrapAlignment wrap_alignment, std::int32_t line_skip
	);

	// @param font Font used for the surface.
	// @param content Content of the text.
	// @return The size of the unscaled text using the given font.
	[[nodiscard]] static V2_int GetSize(Font font, const std::string& content);

	// Convert SDL surface to ptgn::Surface. Loops through all pixels based on format and sets data
	// array.
	Surface(std::shared_ptr<SDL_Surface> surface);
};

} // namespace ptgn