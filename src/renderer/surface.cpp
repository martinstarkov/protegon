#include "renderer/surface.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "SDL_error.h"
#include "SDL_image.h"
#include "SDL_pixels.h"
#include "SDL_surface.h"
#include "SDL_ttf.h"
#include "core/game.h"
#include "core/sdl_instance.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/font.h"
#include "utility/debug.h"
#include "utility/file.h"
#include "utility/handle.h"
#include "utility/log.h"

namespace ptgn {

namespace impl {

void SDL_SurfaceDeleter::operator()(SDL_Surface* surface) const {
	if (game.sdl_instance_->SDLIsInitialized()) {
		SDL_FreeSurface(surface);
	}
}

TextureFormat GetFormatFromSDL(std::uint32_t sdl_format) {
	switch (sdl_format) {
		case SDL_PIXELFORMAT_RGBA32:	[[fallthrough]];
		case SDL_PIXELFORMAT_RGBA8888:	return TextureFormat::RGBA8888;
		case SDL_PIXELFORMAT_RGB24:		[[fallthrough]];
		case SDL_PIXELFORMAT_RGB888:	return TextureFormat::RGB888;
		case SDL_PIXELFORMAT_BGRA32:	[[fallthrough]];
		case SDL_PIXELFORMAT_BGRA8888:	return TextureFormat::BGRA8888;
		case SDL_PIXELFORMAT_BGR24:		[[fallthrough]];
		case SDL_PIXELFORMAT_BGR888:	return TextureFormat::BGR888;
		case SDL_PIXELFORMAT_INDEX8:	[[fallthrough]];
		case SDL_PIXELFORMAT_UNKNOWN:	return TextureFormat::Unknown;
		case SDL_PIXELFORMAT_INDEX1LSB: PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_INDEX1MSB: PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
#ifndef __EMSCRIPTEN__
		case SDL_PIXELFORMAT_INDEX2LSB: PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_INDEX2MSB: PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
#endif
		case SDL_PIXELFORMAT_INDEX4LSB:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_INDEX4MSB:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_RGB332:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_RGB444:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_BGR444:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_RGB555:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_BGR555:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_ARGB4444:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_RGBA4444:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_ABGR4444:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_BGRA4444:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_ARGB1555:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_RGBA5551:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_ABGR1555:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_BGRA5551:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_RGB565:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_BGR565:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_RGBX8888:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_BGRX8888:	  PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		case SDL_PIXELFORMAT_ARGB2101010: PTGN_ERROR("Unsupported sdl format"); [[fallthrough]];
		default:						  PTGN_ERROR("Unrecognized sdl format");
	}
}

} // namespace impl

V2_int Surface::GetSize(Font font, const std::string& content) {
	PTGN_ASSERT(font.IsValid(), "Cannot get size of uninitialized or invalid font");
	V2_int size;
	TTF_SizeUTF8(&font.Get(), content.c_str(), &size.x, &size.y);
	return size;
}

Surface::Surface(std::shared_ptr<SDL_Surface> surface) {
	Create();

	auto& s{ Get() };

	s.format_ = impl::GetFormatFromSDL(surface->format->format);

	if (s.format_ == TextureFormat::Unknown) {
		// Convert format.
		s.format_ = TextureFormat::RGBA8888;
		surface	  = {
			  SDL_ConvertSurfaceFormat(surface.get(), static_cast<std::uint32_t>(s.format_), 0),
			  impl::SDL_SurfaceDeleter{}
		};
		PTGN_ASSERT(surface != nullptr, SDL_GetError());
	}

	PTGN_ASSERT(
		s.format_ != TextureFormat::Unknown, "Cannot create surface with unknown texture format"
	);

	int lock = SDL_LockSurface(surface.get());
	PTGN_ASSERT(lock == 0, "Failed to lock surface when copying pixels");

	s.size_ = { surface->w, surface->h };

	int total_pixels = s.size_.x * s.size_.y;

	s.data_.resize(static_cast<std::size_t>(total_pixels), color::Transparent);

	bool r_mask{ surface->format->Rmask == 0x000000ff };
	bool b_mask{ surface->format->Bmask == 0x000000ff };

	for (int y = 0; y < s.size_.y; ++y) {
		const std::uint8_t* row = static_cast<std::uint8_t*>(surface->pixels) + y * surface->pitch;
		int idx_row				= y * s.size_.x;
		for (int x = 0; x < s.size_.x; ++x) {
			const std::uint8_t* pixel = row + x * surface->format->BytesPerPixel;
			int index				  = idx_row + x;
			PTGN_ASSERT(index < static_cast<int>(s.data_.size()));

			switch (surface->format->BytesPerPixel) {
				case 4: {
					if (r_mask) {
						s.data_[static_cast<std::size_t>(index)] = { pixel[0], pixel[1], pixel[2],
																	 pixel[3] };
					} else if (b_mask) {
						s.data_[static_cast<std::size_t>(index)] = { pixel[2], pixel[1], pixel[0],
																	 pixel[3] };
					} else {
						s.data_[static_cast<std::size_t>(index)] = { pixel[3], pixel[2], pixel[1],
																	 pixel[0] };
					}
					break;
				}
				case 3: {
					if (r_mask) {
						s.data_[static_cast<std::size_t>(index)] = { pixel[0], pixel[1], pixel[2],
																	 255 };
					} else if (b_mask) {
						s.data_[static_cast<std::size_t>(index)] = { pixel[2], pixel[1], pixel[0],
																	 255 };
					} else {
						PTGN_ERROR("Failed to identify mask for fully opaque pixel format");
					}
					break;
				}
				case 1: {
					s.data_[static_cast<std::size_t>(index)] = { 255, 255, 255, pixel[0] };
					break;
				}
				default: PTGN_ERROR("Unsupported texture format"); break;
			}
		}
	}
	PTGN_ASSERT(static_cast<int>(s.data_.size()) == s.size_.x * s.size_.y);

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
	const std::string& content, std::int32_t ptsize, const Color& shading_color,
	std::uint32_t wrap_after_pixels, TextWrapAlignment wrap_alignment, std::int32_t line_skip
) :
	Surface{ std::invoke([&]() {
		PTGN_ASSERT(
			font.IsValid(), "Cannot create a surface with an invalid or uninitialized font"
		);

		auto f = &font.Get();

		TTF_SetFontStyle(f, static_cast<int>(style));
		TTF_SetFontWrappedAlign(f, static_cast<int>(wrap_alignment));
		if (line_skip != std::numeric_limits<std::int32_t>::infinity()) {
		// TODO: Re-enable this for Emscripten once it is supported (SDL_ttf 2.24.0).
#ifndef __EMSCRIPTEN__
			TTF_SetFontLineSkip(f, line_skip);
#endif
		}
		if (ptsize != std::numeric_limits<std::int32_t>::infinity()) {
			TTF_SetFontSize(f, ptsize);
		}

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
	}) } {
}

void Surface::FlipVertically() {
	PTGN_ASSERT(IsValid(), "Cannot vertically flip an invalid or uninitialized surface");
	auto& s{ Get() };
	// TODO: Check that this works as intended (i.e. middle row in odd height images is skipped).
	for (int row{ 0 }; row < s.size_.y / 2; ++row) {
		std::swap_ranges(
			s.data_.begin() + row * s.size_.x, s.data_.begin() + (row + 1) * s.size_.x,
			s.data_.begin() + (s.size_.y - row - 1) * s.size_.x
		);
	}
}

const std::vector<Color>& Surface::GetData() const {
	PTGN_ASSERT(IsValid(), "Cannot get pixel data of an invalid or uninitialized surface");
	return Get().data_;
}

TextureFormat Surface::GetFormat() const {
	PTGN_ASSERT(IsValid(), "Cannot get format of an invalid or uninitialized surface");
	return Get().format_;
}

V2_int Surface::GetSize() const {
	PTGN_ASSERT(IsValid(), "Cannot get size of an invalid or uninitialized surface");
	return Get().size_;
}

Color Surface::GetPixel(const V2_int& coordinate) const {
	PTGN_ASSERT(IsValid(), "Cannot get pixel of an invalid or uninitialized surface");
	auto& s{ Get() };
	int index{ coordinate.y * s.size_.x + coordinate.x };
	PTGN_ASSERT(coordinate.x >= 0, "Coordinate outside of range of grid");
	PTGN_ASSERT(coordinate.y >= 0, "Coordinate outside of range of grid");
	PTGN_ASSERT(index < static_cast<int>(s.data_.size()), "Coordinate outside of range of grid");
	return s.data_[static_cast<std::size_t>(index)];
}

void Surface::ForEachPixel(const std::function<void(const V2_int&, const Color&)>& function) {
	PTGN_ASSERT(IsValid(), "Cannot loop each pixel of an invalid or uninitialized surface");
	auto& s{ Get() };
	for (int j{ 0 }; j < s.size_.y; j++) {
		int idx_row{ j * s.size_.x };
		for (int i{ 0 }; i < s.size_.x; i++) {
			V2_int coordinate{ i, j };
			int index{ idx_row + i };
			PTGN_ASSERT(index < static_cast<int>(s.data_.size()));
			std::invoke(function, coordinate, s.data_[static_cast<std::size_t>(index)]);
		}
	}
}

} // namespace ptgn
