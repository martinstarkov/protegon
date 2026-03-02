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
#include "renderer/primitives/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/file.h"

namespace ptgn::impl {

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

	size = { surface->w, surface->h };

	std::size_t total_pixels{ static_cast<std::size_t>(size.x) * static_cast<std::size_t>(size.y) *
							  kBytesPerPixel };

	pixels.reserve(total_pixels);

	for (int y{ 0 }; y < size.y; ++y) {
		auto row{ static_cast<std::uint8_t*>(surface->pixels) + y * surface->pitch };
		for (int x{ 0 }; x < size.x; ++x) {
			auto pixel{ row + static_cast<std::size_t>(x) * kBytesPerPixel };
			for (std::size_t b{ 0 }; b < kBytesPerPixel; ++b) {
				pixels.push_back(pixel[b]);
			}
		}
	}

	SDL_UnlockSurface(surface);
	SDL_DestroySurface(surface);
}

Surface::Surface(const path& filepath) :
	Surface{ std::invoke([&]() {
		PTGN_ASSERT(
			FileExists(filepath),
			"Cannot create surface from a nonexistent filepath: ", filepath.string()
		);
		// Freed by Surface constructor.
		SDL_Surface* sdl_surface{ IMG_Load(filepath.string().c_str()) };
		PTGN_ASSERT(sdl_surface != nullptr, SDL_GetError());
		return sdl_surface;
	}) } {}

void Surface::FlipVertically() {
	PTGN_ASSERT(!pixels.empty(), "Cannot vertically flip an empty surface");
	// TODO: Check that this works as intended (i.e. middle row in odd height images is skipped).
	for (int row{ 0 }; row < size.y / 2; ++row) {
		std::swap_ranges(
			pixels.begin() + row * size.x, pixels.begin() + (row + 1) * size.x,
			pixels.begin() + (size.y - row - 1) * size.x
		);
	}
}

Color Surface::GetPixel(V2_int coordinate) const {
	PTGN_ASSERT(coordinate.x >= 0, "X Coordinate outside of range of grid");
	PTGN_ASSERT(coordinate.y >= 0, "Y Coordinate outside of range of grid");
	PTGN_ASSERT(coordinate.x < size.x, "X Coordinate outside of range of grid");
	PTGN_ASSERT(coordinate.y < size.y, "Y Coordinate outside of range of grid");
	auto index{ (static_cast<std::size_t>(coordinate.y) * static_cast<std::size_t>(size.x) +
				 static_cast<std::size_t>(coordinate.x)) *
				kBytesPerPixel };
	return GetPixel(index);
}

Color Surface::GetPixel(std::size_t index) const {
	PTGN_ASSERT(!pixels.empty(), "Cannot get pixel of an empty surface");
	PTGN_ASSERT(index < pixels.size(), "Coordinate outside of range of grid");
	index *= kBytesPerPixel;
	if constexpr (kBytesPerPixel == 4) {
		PTGN_ASSERT(index + 3 < pixels.size(), "Coordinate outside of range of grid");
		return { pixels[index + 0], pixels[index + 1], pixels[index + 2], pixels[index + 3] };
	} else if constexpr (kBytesPerPixel == 3) {
		PTGN_ASSERT(index + 2 < pixels.size(), "Coordinate outside of range of grid");
		return { pixels[index + 0], pixels[index + 1], pixels[index + 2], 255 };
	} else if constexpr (kBytesPerPixel == 1) {
		PTGN_ASSERT(index < pixels.size(), "Coordinate outside of range of grid");
		return { 255, 255, 255, pixels[index] };
	} else {
		PTGN_ERROR("Unsupported texture format");
	}
}

void Surface::ForEachPixel(const std::function<void(V2_int, Color)>& function) const {
	PTGN_ASSERT(!pixels.empty(), "Cannot loop through each pixel of an empty surface");
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