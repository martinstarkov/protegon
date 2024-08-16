#include "protegon/surface.h"

#include <algorithm>

#include "SDL.h"
#include "SDL_image.h"
#include "utility/debug.h"

namespace ptgn {

Surface::Surface(const std::shared_ptr<SDL_Surface>& raw_surface, ImageFormat format) {
	PTGN_ASSERT(format != ImageFormat::Unknown, "Cannot create surface with unknown image format");

	if (!IsValid()) {
		instance_ = std::make_shared<impl::SurfaceInstance>();
	}
	instance_->format_ = format;

	std::shared_ptr<SDL_Surface> surface = { SDL_ConvertSurfaceFormat(
												 raw_surface.get(),
												 static_cast<std::uint32_t>(instance_->format_), 0
											 ),
											 SDL_FreeSurface };
	PTGN_ASSERT(surface != nullptr, SDL_GetError());

	int lock = SDL_LockSurface(surface.get());
	PTGN_ASSERT(lock == 0, "Failed to lock surface when copying pixels");

	instance_->size_ = { surface->w, surface->h };

	int total_pixels = instance_->size_.x * instance_->size_.y;

	instance_->data_.resize(total_pixels, color::Transparent);

	bool r_mask{ surface->format->Rmask == 0x000000ff };

	for (int y = 0; y < instance_->size_.y; ++y) {
		std::uint8_t* row	= (std::uint8_t*)surface->pixels + y * surface->pitch;
		std::size_t idx_row = y * instance_->size_.x;
		for (int x = 0; x < instance_->size_.x; ++x) {
			std::uint8_t* pixel = row + x * surface->format->BytesPerPixel;
			std::size_t index	= idx_row + x;
			PTGN_ASSERT(index < instance_->data_.size());

			switch (surface->format->BytesPerPixel) {
				case 4: {
					if (r_mask) {
						instance_->data_[index] = { pixel[1], pixel[2], pixel[3], pixel[0] };
					} else {
						instance_->data_[index] = { pixel[3], pixel[2], pixel[1], pixel[0] };
					}
					break;
				}
				case 3: {
					if (r_mask) {
						instance_->data_[index] = { pixel[0], pixel[1], pixel[2], 255 };
					} else {
						instance_->data_[index] = { pixel[2], pixel[1], pixel[0], 255 };
					}
					break;
				}
				case 1: {
					instance_->data_[index] = { 255, 255, 255, pixel[0] };
					break;
				}
				default: PTGN_ERROR("Unsupported image format"); break;
			}
		}
	}
	PTGN_ASSERT(instance_->data_.size() == instance_->size_.x * instance_->size_.y);

	SDL_UnlockSurface(surface.get());
}

Surface::Surface(const path& image_path) :
	Surface{ [&]() -> std::shared_ptr<SDL_Surface> {
		PTGN_ASSERT(
			FileExists(image_path),
			"Cannot create surface from a nonexistent image path: ", image_path.string()
		);
		std::shared_ptr<SDL_Surface> raw_surface = { IMG_Load(image_path.string().c_str()),
													 SDL_FreeSurface };
		PTGN_ASSERT(raw_surface != nullptr, IMG_GetError());
		return raw_surface;
	}() } {}

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

	for (int j = 0; j < instance_->size_.y; j++) {
		int idx_row = j * instance_->size_.x;
		for (int i = 0; i < instance_->size_.x; i++) {
			V2_int coordinate{ i, j };
			std::size_t index = idx_row + i;
			PTGN_ASSERT(index < instance_->data_.size());
			function(coordinate, instance_->data_[index]);
		}
	}
}

} // namespace ptgn