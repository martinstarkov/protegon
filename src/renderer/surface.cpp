#include "renderer/surface.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "SDL_error.h"
#include "SDL_image.h"
#include "SDL_pixels.h"
#include "SDL_surface.h"
#include "SDL_ttf.h"
#include "core/sdl_instance.h"
#include "renderer/color.h"
#include "utility/file.h"
#include "renderer/font.h"
#include "core/game.h"
#include "utility/log.h"
#include "math/vector2.h"
#include "utility/debug.h"
#include "utility/handle.h"

namespace ptgn {

namespace impl {

void SDL_SurfaceDeleter::operator()(SDL_Surface* surface) const {
	if (game.sdl_instance_->SDLIsInitialized()) {
		SDL_FreeSurface(surface);
	}
}

} // namespace impl

V2_int Surface::GetSize(Font font, const std::string& content) {
	V2_int size;
	TTF_SizeUTF8(&font.Get(), content.c_str(), &size.x, &size.y);
	return size;
}

Surface::Surface(const std::shared_ptr<SDL_Surface>& raw_surface, ImageFormat format) {
	PTGN_ASSERT(format != ImageFormat::Unknown, "Cannot create surface with unknown image format");
	Create();
	auto& s{ Get() };
	s.format_ = format;

	std::shared_ptr<SDL_Surface> surface = {
		SDL_ConvertSurfaceFormat(raw_surface.get(), static_cast<std::uint32_t>(s.format_), 0),
		impl::SDL_SurfaceDeleter{}
	};
	PTGN_ASSERT(surface != nullptr, SDL_GetError());

	int lock = SDL_LockSurface(surface.get());
	PTGN_ASSERT(lock == 0, "Failed to lock surface when copying pixels");

	s.size_ = { surface->w, surface->h };

	int total_pixels = s.size_.x * s.size_.y;

	s.data_.resize(total_pixels, color::Transparent);

	bool r_mask{ surface->format->Rmask == 0x000000ff };

	for (int y = 0; y < s.size_.y; ++y) {
		const std::uint8_t* row = (std::uint8_t*)surface->pixels + y * surface->pitch;
		std::size_t idx_row		= static_cast<std::size_t>(y) * s.size_.x;
		for (int x = 0; x < s.size_.x; ++x) {
			const std::uint8_t* pixel = row + x * surface->format->BytesPerPixel;
			std::size_t index		  = idx_row + x;
			PTGN_ASSERT(index < s.data_.size());

			switch (surface->format->BytesPerPixel) {
				case 4: {
					if (r_mask) {
						s.data_[index] = { pixel[1], pixel[2], pixel[3], pixel[0] };
					} else {
						s.data_[index] = { pixel[3], pixel[2], pixel[1], pixel[0] };
					}
					break;
				}
				case 3: {
					if (r_mask) {
						s.data_[index] = { pixel[0], pixel[1], pixel[2], 255 };
					} else {
						s.data_[index] = { pixel[2], pixel[1], pixel[0], 255 };
					}
					break;
				}
				case 1: {
					s.data_[index] = { 255, 255, 255, pixel[0] };
					break;
				}
				default: PTGN_ERROR("Unsupported image format"); break;
			}
		}
	}
	PTGN_ASSERT(s.data_.size() == s.size_.x * s.size_.y);

	SDL_UnlockSurface(surface.get());
}

Surface::Surface(const path& image_path) :
	Surface{ std::invoke([&]() {
		PTGN_ASSERT(
			FileExists(image_path),
			"Cannot create surface from a nonexistent image path: ", image_path.string()
		);
		std::shared_ptr<SDL_Surface> raw_surface = { IMG_Load(image_path.string().c_str()),
													 impl::SDL_SurfaceDeleter{} };
		PTGN_ASSERT(raw_surface != nullptr, IMG_GetError());
		return raw_surface;
	}) } {}

Surface::Surface(
	Font& font, FontStyle style, const Color& text_color, FontRenderMode mode,
	const std::string& content, const Color& shading_color, std::uint32_t wrap_after_pixels
) :
	Surface{ std::invoke([&]() {
		auto f = &font.Get();

		TTF_SetFontStyle(f, static_cast<int>(style));

		std::shared_ptr<SDL_Surface> surface;

		SDL_Color tc{ text_color.r, text_color.g, text_color.b, text_color.a };

		switch (mode) {
			case FontRenderMode::Solid:
				surface = std::shared_ptr<SDL_Surface>{
					TTF_RenderUTF8_Solid_Wrapped(f, content.c_str(), tc, wrap_after_pixels),
					impl::SDL_SurfaceDeleter{}
				};
				break;
			case FontRenderMode::Shaded: {
				SDL_Color sc{ shading_color.r, shading_color.g, shading_color.b, shading_color.a };
				surface = std::shared_ptr<SDL_Surface>{
					TTF_RenderUTF8_Shaded_Wrapped(f, content.c_str(), tc, sc, wrap_after_pixels),
					impl::SDL_SurfaceDeleter{}
				};
				break;
			}
			case FontRenderMode::Blended:
				surface = std::shared_ptr<SDL_Surface>{
					TTF_RenderUTF8_Blended_Wrapped(f, content.c_str(), tc, wrap_after_pixels),
					impl::SDL_SurfaceDeleter{}
				};
				break;
			default:
				PTGN_ERROR(
					"Unrecognized render mode given when creating surface from font information"
				);
		}

		PTGN_ASSERT(surface != nullptr, "Failed to create surface for given font information");

		return surface;
	}) } {}

void Surface::FlipVertically() {
	auto& s{ Get() };
	// TODO: Check that this works as intended (i.e. middle row in odd height images is skipped).
	for (std::size_t row = 0; row < s.size_.y / 2; ++row) {
		std::swap_ranges(
			s.data_.begin() + row * s.size_.x, s.data_.begin() + (row + 1) * s.size_.x,
			s.data_.begin() + (s.size_.y - row - 1) * s.size_.x
		);
	}
}

const std::vector<Color>& Surface::GetData() const {
	return Get().data_;
}

ImageFormat Surface::GetImageFormat() const {
	return Get().format_;
}

V2_int Surface::GetSize() const {
	return Get().size_;
}

void Surface::ForEachPixel(const std::function<void(const V2_int&, const Color&)>& function) {
	auto& s{ Get() };
	for (int j = 0; j < s.size_.y; j++) {
		std::size_t idx_row = static_cast<std::size_t>(j) * s.size_.x;
		for (int i = 0; i < s.size_.x; i++) {
			V2_int coordinate{ i, j };
			std::size_t index = idx_row + i;
			PTGN_ASSERT(index < s.data_.size());
			std::invoke(function, coordinate, s.data_[index]);
		}
	}
}

} // namespace ptgn