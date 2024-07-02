#include "protegon/surface.h"

#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>

#include "protegon/debug.h"

namespace ptgn {

namespace impl {

SurfaceInstance::SurfaceInstance(const path& image_path, ImageFormat format) : format_{ format } {
	PTGN_CHECK(FileExists(image_path), "Cannot create surface from a nonexistent image path");
	std::shared_ptr<SDL_Surface> raw_surface = { IMG_Load(image_path.string().c_str()),
												 SDL_FreeSurface };
	if (raw_surface == nullptr) {
		PTGN_ERROR(IMG_GetError());
		PTGN_ASSERT(false, "Failed to create surface from image path");
	}

	std::shared_ptr<SDL_Surface> surface = {
		SDL_ConvertSurfaceFormat(raw_surface.get(), static_cast<std::uint32_t>(format_), 0),
		SDL_FreeSurface
	};
	if (surface == nullptr) {
		PTGN_ERROR(SDL_GetError());
		PTGN_ASSERT(false, "Failed to convert surface to the given format");
	}

	pitch_			 = surface->pitch;
	bytes_per_pixel_ = surface->format->BytesPerPixel;
	size_			 = { surface->w, surface->h };

	int length = size_.x * size_.y * bytes_per_pixel_;

	std::uint8_t* char_buffer = static_cast<std::uint8_t*>(surface->pixels);
	/* create a vector by copying out the contents of charBuf */
	data_ = std::vector<std::uint8_t>(char_buffer, char_buffer + length);
}

} // namespace impl

Surface::Surface(const path& image_path, ImageFormat format) {
	instance_ =
		std::shared_ptr<impl::SurfaceInstance>(new impl::SurfaceInstance(image_path, format));
}

void Surface::FlipVertically() {
	PTGN_CHECK(IsValid(), "Cannot flip surface vertically if it is uninitialized or destroyed");
	// TODO: Check that this works as intended (i.e. middle row in odd height images is skipped).
	for (std::size_t row = 0; row < instance_->size_.y / 2; ++row) {
		std::swap_ranges(
			instance_->data_.begin() + row * instance_->pitch_,
			instance_->data_.begin() + (row + 1) * instance_->pitch_,
			instance_->data_.begin() + (instance_->size_.y - row - 1) * instance_->pitch_
		);
	}
}

std::int32_t Surface::GetBytesPerPixel() const {
	PTGN_CHECK(IsValid(), "Cannot get bytes per pixel of an uninitialized or destroyed surface");
	return instance_->bytes_per_pixel_;
}

std::int32_t Surface::GetPitch() const {
	PTGN_CHECK(IsValid(), "Cannot get pitch of an uninitialized or destroyed surface");
	return instance_->pitch_;
}

const std::vector<std::uint8_t>& Surface::GetData() const {
	PTGN_CHECK(IsValid(), "Cannot get data of an uninitialized or destroyed surface");
	return instance_->data_;
}

ImageFormat Surface::GetImageFormat() const {
	PTGN_CHECK(IsValid(), "Cannot get image format of an uninitialized or destroyed surface");
	return instance_->format_;
}

V2_int Surface::GetSize() const {
	PTGN_CHECK(IsValid(), "Cannot get size of an uninitialized or destroyed surface");
	return instance_->size_;
}

// void Surface::ForEachPixel(std::function<void(const V2_int&, const Color&)> function) {
//	PTGN_CHECK(IsValid(), "Cannot loop through each pixel of uninitialized or destroyed surface");
//	Lock();
//
//	const V2_int size{ GetSize() };
//
//	for (int i = 0; i < size.x; i++) {
//		for (int j = 0; j < size.y; j++) {
//			V2_int coordinate{ i, j };
//			function(coordinate, GetColor(coordinate));
//		}
//	}
//
//	Unlock();
// }
//
//  void Surface::Lock() {
//	PTGN_CHECK(IsValid(), "Cannot lock an uninitialized or destroyed surface");
//	int success = SDL_LockSurface(instance_.get());
//	if (success != 0) {
//		PTGN_ERROR(SDL_GetError());
//		PTGN_ASSERT(false, "Failed to lock surface");
//	}
//  }
//
//  void Surface::Unlock() {
//	PTGN_CHECK(IsValid(), "Cannot unlock an uninitialized or destroyed surface");
//	SDL_UnlockSurface(instance_.get());
//  }
//
//  Color Surface::GetColor(const V2_int& coordinate) {
//	PTGN_CHECK(IsValid(), "Cannot get color of an uninitialized or destroyed surface");
//	return GetColorData(GetPixelData(coordinate));
//  }
//
//  Color Surface::GetColorData(std::uint32_t pixel_data) {
//	PTGN_CHECK(IsValid(), "Cannot get color data of an uninitialized or destroyed surface");
//	Color color;
//	SDL_GetRGB(pixel_data, instance_.get()->format, &color.r, &color.g, &color.b);
//	return color;
//  }
//
//  std::uint32_t Surface::GetPixelData(const V2_int& coordinate) {
//	PTGN_CHECK(IsValid(), "Cannot get pixel data of an uninitialized or destroyed surface");
//	int bpp			= instance_.get()->format->BytesPerPixel;
//	std::uint8_t* p = (std::uint8_t*)instance_.get()->pixels +
//					  coordinate.y * instance_.get()->pitch + coordinate.x * bpp;
//	PTGN_CHECK(p != nullptr, "Failed to find coordinate pixel data");
//	switch (bpp) {
//		case 1: return *p; break;
//		case 2: return *(std::uint16_t*)p; break;
//
//		case 3:
//			if constexpr (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
//				return p[0] << 16 | p[1] << 8 | p[2];
//			} else {
//				return p[0] | p[1] << 8 | p[2] << 16;
//			}
//			break;
//
//		case 4:	 return *(uint32_t*)p; break;
//
//		default: PTGN_ASSERT(false, "Failed to find coordinate pixel data"); return 0;
//	}
//  }

} // namespace ptgn
