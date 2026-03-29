#include "renderer/image/surface.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/primitives/color.h"

namespace ptgn::impl {

SDL_Surface* LoadSurface(const path& filepath) {
	PTGN_ASSERT(
		FileExists(filepath),
		"Cannot create surface from a nonexistent filepath: ", filepath.string()
	);
	SDL_Surface* sdl_surface{ IMG_Load(filepath.string().c_str()) };
	PTGN_ASSERT(sdl_surface != nullptr, SDL_GetError());
	return sdl_surface;
}

Surface::Surface(SDL_Surface* sdl_surface) {
	PTGN_ASSERT(sdl_surface != nullptr, "Cannot create surface from nullptr");

	// TODO: In the future, instead of converting all formats to RGBA, figure out how to deal with
	// Windows and MacOS discrepencies between image formats and SDL surface formats to enable the
	// use of RGB888 format (faster for JPGs). When I was using this approach in the past, MacOS had
	// an issue rendering JPG images as it perceived them as having 4 bytes per pixel with BGRA8888
	// format even though SDL said they were RGB888. Whereas on Windows, the same JPGs opened as 3
	// channel RGB888 surfaces as expected.
	SDL_Surface* surface = SDL_ConvertSurface(sdl_surface, SDL_PixelFormat::SDL_PIXELFORMAT_RGBA32);

	PTGN_ASSERT(surface != nullptr, SDL_GetError());

	PTGN_ASSERT(
		SDL_GetPixelFormatDetails(surface->format)->bytes_per_pixel == kBytesPerPixel,
		"Failed to convert surface to RGBA32"
	);

	SDL_DestroySurface(sdl_surface);

	bool lock{ SDL_LockSurface(surface) };
	PTGN_ASSERT(lock, "Failed to lock surface when copying pixels");

	size_ = { surface->w, surface->h };

	std::size_t total_pixels{ static_cast<std::size_t>(size_.x) *
							  static_cast<std::size_t>(size_.y) * kBytesPerPixel };

	pixels_.reserve(total_pixels);

	for (int y{ 0 }; y < size_.y; ++y) {
		auto row_index{ static_cast<std::uint8_t*>(surface->pixels) + y * surface->pitch };
		for (int x{ 0 }; x < size_.x; ++x) {
			auto pixel{ row_index + static_cast<std::size_t>(x) * kBytesPerPixel };
			for (std::size_t b{ 0 }; b < kBytesPerPixel; ++b) {
				pixels_.push_back(pixel[b]);
			}
		}
	}

	SDL_UnlockSurface(surface);
	SDL_DestroySurface(surface);
}

Surface::Surface(const path& filepath) :
	Surface{ LoadSurface(filepath)
			 /* SDL_Surface destroyed by Surface constructor. */ } {}

void Surface::FlipVertically() {
	PTGN_ASSERT(!pixels_.empty(), "Cannot vertically flip an empty surface");
	// TODO: Check that this works as intended (i.e. middle row in odd height images is skipped).
	for (std::size_t row{ 0 }; row < static_cast<std::size_t>(size_.y) / 2; ++row) {
		std::swap_ranges(
			pixels_.begin() + row * size_.x, pixels_.begin() + (row + 1) * size_.x,
			pixels_.begin() + (size_.y - row - 1) * size_.x
		);
	}
}

Color Surface::GetPixel(V2_int coordinate) const {
	PTGN_ASSERT(
		coordinate.x >= 0 && coordinate.x < size_.x, "X Coordinate '", coordinate.x,
		"' outside of surface width: ", size_.x
	);
	PTGN_ASSERT(
		coordinate.y >= 0 && coordinate.y < size_.y, "Y Coordinate '", coordinate.y,
		"' outside of surface height: ", size_.y
	);
	auto index{ (static_cast<std::size_t>(coordinate.y) * static_cast<std::size_t>(size_.x) +
				 static_cast<std::size_t>(coordinate.x)) *
				kBytesPerPixel };
	return GetPixel(index);
}

Color Surface::GetPixel(std::size_t index) const {
	PTGN_ASSERT(!pixels_.empty(), "Cannot get pixel of an empty surface");
	PTGN_ASSERT(index < pixels_.size(), "Index outside of range of grid");
	index *= kBytesPerPixel;
	if constexpr (kBytesPerPixel == 4) {
		PTGN_ASSERT(index + 3 < pixels_.size(), "Index outside of range of grid");
		return { pixels_[index + 0], pixels_[index + 1], pixels_[index + 2], pixels_[index + 3] };
	} else if constexpr (kBytesPerPixel == 3) {
		PTGN_ASSERT(index + 2 < pixels_.size(), "Index outside of range of grid");
		return { pixels_[index + 0], pixels_[index + 1], pixels_[index + 2], 255 };
	} else if constexpr (kBytesPerPixel == 1) {
		PTGN_ASSERT(index < pixels_.size(), "Index outside of range of grid");
		return { 255, 255, 255, pixels_[index] };
	} else {
		PTGN_ERROR("Unsupported texture format");
	}
}

V2_int Surface::GetSize() const {
	return size_;
}

const std::uint8_t* Surface::Data() const {
	return pixels_.data();
}

} // namespace ptgn::impl