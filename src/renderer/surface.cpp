#include "protegon/surface.h"

#include <algorithm>

#include "SDL.h"
#include "SDL_image.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

SurfaceInstance::SurfaceInstance(const path& image_path, ImageFormat format) : format_{ format } {
	PTGN_ASSERT(FileExists(image_path), "Cannot create surface from a nonexistent image path");
	PTGN_ASSERT(format_ != ImageFormat::Unknown, "Cannot create surface with unknown image format");
	std::shared_ptr<SDL_Surface> raw_surface = { IMG_Load(image_path.string().c_str()),
												 SDL_FreeSurface };
	PTGN_ASSERT(raw_surface != nullptr, IMG_GetError());

	std::shared_ptr<SDL_Surface> surface = {
		SDL_ConvertSurfaceFormat(raw_surface.get(), static_cast<std::uint32_t>(format_), 0),
		SDL_FreeSurface
	};
	PTGN_ASSERT(surface != nullptr, SDL_GetError());

	int lock = SDL_LockSurface(surface.get());
	PTGN_ASSERT(lock == 0, "Failed to lock surface when copying pixels");

	size_ = { surface->w, surface->h };

	int total_pixels = size_.x * size_.y;

	data_.resize(total_pixels, color::Black);

	for (int y = 0; y < size_.y; ++y) {
		std::uint8_t* row	= (std::uint8_t*)surface->pixels + y * surface->pitch;
		std::size_t idx_row = y * size_.x;
		for (int x = 0; x < size_.x; ++x) {
			std::uint8_t* pixel = row + x * surface->format->BytesPerPixel;
			std::size_t index	= idx_row + x;
			PTGN_ASSERT(index < data_.size());
			switch (surface->format->BytesPerPixel) {
				case 4: // RGBA8888
					data_[index] = { pixel[3], pixel[2], pixel[1], pixel[0] };
					break;
				case 3:													  // RGB888
					data_[index] = { pixel[2], pixel[1], pixel[0], 255 }; // Assume opaque
					break;
				default: PTGN_ERROR("Unsupported image format"); break;
			}
		}
	}
	PTGN_ASSERT(data_.size() == size_.x * size_.y);

	SDL_UnlockSurface(surface.get());
}

} // namespace impl

Surface::Surface(const path& image_path, ImageFormat format) {
	instance_ = std::make_shared<impl::SurfaceInstance>(image_path, format);
}

void Surface::FlipVertically() {
	PTGN_ASSERT(IsValid(), "Cannot flip surface vertically if it is uninitialized or destroyed");
	// TODO: Check that this works as intended (i.e. middle row in odd height images is skipped).
	for (std::size_t row = 0; row < instance_->size_.y / 2; ++row) {
		std::swap_ranges(
			instance_->data_.begin() + row * instance_->size_.x,
			instance_->data_.begin() + (row + 1) * instance_->size_.x,
			instance_->data_.begin() + (instance_->size_.y - row - 1) * instance_->size_.x
		);
	}
}

const std::vector<Color>& Surface::GetData() const {
	PTGN_ASSERT(IsValid(), "Cannot get data of an uninitialized or destroyed surface");
	return instance_->data_;
}

ImageFormat Surface::GetImageFormat() const {
	PTGN_ASSERT(IsValid(), "Cannot get image format of an uninitialized or destroyed surface");
	return instance_->format_;
}

V2_int Surface::GetSize() const {
	PTGN_ASSERT(IsValid(), "Cannot get size of an uninitialized or destroyed surface");
	return instance_->size_;
}

void Surface::ForEachPixel(std::function<void(const V2_int&, const Color&)> function) {
	PTGN_ASSERT(IsValid(), "Cannot loop through each pixel of uninitialized or destroyed surface");

	for (std::size_t j = 0; j < instance_->size_.y; j++) {
		std::size_t idx_row = j * instance_->size_.x;
		for (std::size_t i = 0; i < instance_->size_.x; i++) {
			V2_int coordinate{ i, j };
			std::size_t index = idx_row + i;
			PTGN_ASSERT(index < instance_->data_.size());
			function(coordinate, instance_->data_[index]);
		}
	}
}

} // namespace ptgn