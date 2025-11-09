#include "renderer/image/surface.h"

#include "SDL_error.h"
#include "SDL_image.h"
#include "SDL_pixels.h"
#include "SDL_surface.h"
#include "core/assert.h"

namespace ptgn::impl {

Surface::Surface(const path& filepath) {
	PTGN_ASSERT(
		FileExists(filepath),
		"Cannot create surface from a nonexistent filepath: ", filepath.string()
	);
	// Freed by Surface constructor.
	SDL_Surface* sdl_surface{ IMG_Load(filepath.string().c_str()) };

	PTGN_ASSERT(sdl_surface != nullptr, IMG_GetError());

	// TODO: In the future, instead of converting all formats to RGBA, figure out how to deal with
	// Windows and MacOS discrepencies between image formats and SDL2 surface formats to enable the
	// use of RGB888 format (faster for JPGs). When I was using this approach in the past, MacOS had
	// an issue rendering JPG images as it perceived them as having 4 bytes per pixel with BGRA8888
	// format even though SDL2 said they were RGB888. Whereas on Windows, the same JPGs opened as 3
	// channel RGB888 surfaces as expected.
	SDL_Surface* surface = SDL_ConvertSurfaceFormat(
		sdl_surface, static_cast<std::uint32_t>(SDL_PIXELFORMAT_RGBA32), 0
	);

	PTGN_ASSERT(surface != nullptr, SDL_GetError());
	PTGN_ASSERT(
		surface->format->BytesPerPixel == bytes_per_pixel, "Failed to convert surface to RGBA32"
	);

	SDL_FreeSurface(sdl_surface);

	int lock{ SDL_LockSurface(surface) };
	PTGN_ASSERT(lock == 0, "Failed to lock surface when copying pixels");

	size = { surface->w, surface->h };

	std::size_t total_pixels{ static_cast<std::size_t>(size.x) * static_cast<std::size_t>(size.y) *
							  bytes_per_pixel };

	data.reserve(total_pixels);

	for (int y{ 0 }; y < size.y; ++y) {
		auto row{ static_cast<std::uint8_t*>(surface->pixels) + y * surface->pitch };
		for (int x{ 0 }; x < size.x; ++x) {
			auto pixel{ row + static_cast<std::size_t>(x) * bytes_per_pixel };
			for (std::size_t b{ 0 }; b < bytes_per_pixel; ++b) {
				data.push_back(pixel[b]);
			}
		}
	}

	SDL_UnlockSurface(surface);
	SDL_FreeSurface(surface);
}

void Surface::FlipVertically() {
	PTGN_ASSERT(!data.empty(), "Cannot vertically flip an empty surface");
	// TODO: Check that this works as intended (i.e. middle row in odd height images is skipped).
	for (int row{ 0 }; row < size.y / 2; ++row) {
		std::swap_ranges(
			data.begin() + row * size.x, data.begin() + (row + 1) * size.x,
			data.begin() + (size.y - row - 1) * size.x
		);
	}
}

Color Surface::GetPixel(const V2_int& coordinate) const {
	PTGN_ASSERT(coordinate.x >= 0, "X Coordinate outside of range of grid");
	PTGN_ASSERT(coordinate.y >= 0, "Y Coordinate outside of range of grid");
	PTGN_ASSERT(coordinate.x < size.x, "X Coordinate outside of range of grid");
	PTGN_ASSERT(coordinate.y < size.y, "Y Coordinate outside of range of grid");
	auto index{ (static_cast<std::size_t>(coordinate.y) * static_cast<std::size_t>(size.x) +
				 static_cast<std::size_t>(coordinate.x)) *
				bytes_per_pixel };
	return GetPixel(index);
}

Color Surface::GetPixel(std::size_t index) const {
	PTGN_ASSERT(!data.empty(), "Cannot get pixel of an empty surface");
	PTGN_ASSERT(index < data.size(), "Coordinate outside of range of grid");
	index *= bytes_per_pixel;
	if constexpr (bytes_per_pixel == 4) {
		PTGN_ASSERT(index + 3 < data.size(), "Coordinate outside of range of grid");
		return { data[index + 0], data[index + 1], data[index + 2], data[index + 3] };
	} else if constexpr (bytes_per_pixel == 3) {
		PTGN_ASSERT(index + 2 < data.size(), "Coordinate outside of range of grid");
		return { data[index + 0], data[index + 1], data[index + 2], 255 };
	} else if constexpr (bytes_per_pixel == 1) {
		PTGN_ASSERT(index < data.size(), "Coordinate outside of range of grid");
		return { 255, 255, 255, data[index] };
	} else {
		PTGN_ERROR("Unsupported texture format");
	}
}

void Surface::ForEachPixel(const std::function<void(const V2_int&, const Color&)>& function) const {
	PTGN_ASSERT(!data.empty(), "Cannot loop through each pixel of an empty surface");
	PTGN_ASSERT(function != nullptr, "Invalid loop function");
	for (int j{ 0 }; j < size.y; j++) {
		auto idx_row{ static_cast<std::size_t>(j) * static_cast<std::size_t>(size.x) };
		for (int i{ 0 }; i < size.x; i++) {
			V2_int coordinate{ i, j };
			auto index{ idx_row + static_cast<std::size_t>(i) };
			function(coordinate, GetPixel(index));
		}
	}
}

} // namespace ptgn::impl