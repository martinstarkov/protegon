#include "Image.h"

#include <SDL_image.h>
#include <SDL.h>
#include <iterator>
#include <cstdint>
#include <cassert>

#include "renderer/TextureManager.h"

namespace internal {

// Convert an SDL surface coordinate to a 4 byte integer value containg the RGBA32 color of the pixel.
static std::uint32_t GetSurfacePixelColor(SDL_Surface* image, V2_int position) {
	// Source: http://sdl.beuc.net/sdl.wiki/Pixel_Access
	auto bytes_per_pixel = image->format->BytesPerPixel;
	auto row = position.y * image->pitch;
	auto column = position.x * bytes_per_pixel;
	auto index = static_cast<std::size_t>(row) + static_cast<std::size_t>(column);
	auto pixel_address = static_cast<std::uint8_t*>(image->pixels) + index;
	switch (bytes_per_pixel) {
		case 1:
			return *pixel_address;
			break;
		case 2:
			return *(std::uint16_t*)pixel_address;
			break;
		case 3:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
				return pixel_address[0] << 16 | pixel_address[1] << 8 | pixel_address[2];
			else
				return pixel_address[0] | pixel_address[1] << 8 | pixel_address[2] << 16;
			break;
		case 4:
			return *(std::uint32_t*)pixel_address;
			break;
		default:
			return 0; // Shouldn't happen, but avoids warnings.
	}
}

} // namespace internal

namespace engine {

// TODO: FIX! Make sure SetPixel is uncommented.
//Image::Image(const char* path) {
//	// TODO: Add support for formats other than PNG.
//	SDL_Surface* temp_surface = IMG_Load(path);
//	if (!temp_surface) {
//		printf("IMG_Load: %s\n", IMG_GetError());
//		assert(false && "Failed to retrieve image data");
//	}
//	SDL_Surface* surface = SDL_ConvertSurfaceFormat(temp_surface, SDL_PIXELFORMAT_RGBA32, 0);
//	if (!surface) {
//		printf("IMG_Load: %s\n", IMG_GetError());
//		assert(false && "Failed to convert surface data format");
//	}
//	SDL_FreeSurface(temp_surface);
//	assert(surface != nullptr && "Failed to convert image to RGBA format");
//	size_.x = surface->w;
//	size_.y = surface->h;
//	pixels_.resize(static_cast<std::size_t>(size_.x) * static_cast<std::size_t>(size_.y), Color{});
//	for (auto j = 0; j < size_.y; ++j) {
//		for (auto i = 0; i < size_.x; ++i) {
//			//SetPixel({ i, j }, engine::TextureManager::GetTexturePixel(surface, { i, j }));
//		}
//	}
//	SDL_FreeSurface(surface);
//}

Image::Image(const std::vector<Color>& pixels, const V2_int& size, const V2_int& relative_position) : 
	pixels_{ pixels }, 
	size_{ size }, 
	original_size_{ size }, 
	position_{ relative_position } {}

Color Image::GetPixel(const V2_int& position) const {
	auto index = position.y * size_.x + position.x;
	assert(pixels_.size() > 0 && 
		position.x < size_.x && 
		position.y < size_.y && 
		index < pixels_.size() && 
		"Pixel out of range of image size");
	return pixels_[index];
}

V2_int Image::GetSize() const {
	return size_; 
}

V2_int Image::GetOriginalSize() const {
	return original_size_;
}

V2_int Image::GetPosition() const {
	return position_;
}

Image Image::GetSubImage(const V2_int& top_left, const V2_int& bottom_right) const {
	std::vector<Color> sub_pixels;
	// Add { 1, 1 } since taking difference omits one row and column of pixels.
	V2_int sub_image_size = bottom_right - top_left + V2_int{ 1, 1 };
	auto sub_pixels_size = sub_image_size.x * sub_image_size.y;
	sub_pixels.reserve(sub_pixels_size);
	// Find indexes of first and last points to cut down on loop time.
	auto first_index = top_left.y * size_.x + top_left.x;
	auto last_index = bottom_right.y * size_.x + bottom_right.x;
	assert(first_index >= 0 && 
		first_index < pixels_.size() && 
		"Top left coordinate out of range of image pixels");
	assert(last_index >= 0 && 
		last_index < pixels_.size() && 
		"Top right coordinate out of range of image pixels");
	for (auto i = first_index; i < last_index + 1; ++i) {
		auto mod_index = i % size_.x;
		if (mod_index >= top_left.x && mod_index <= bottom_right.x) {
			sub_pixels.push_back(pixels_[i]);
		}
	}
	return Image{ std::move(sub_pixels), sub_image_size, top_left };
}
/*
void Image::AddSide(Side side, Color color) {
	if (side.IsHorizontal()) {
		// Offset relative to the beginning of the pixels vector.
		int offset = 0;
		if (side.IsLeft()) {
			// Offset relative position.
			position_.x -= 1;
			offset = 0;
		} else if (side.IsRight()) {
			offset = size_.x;
		}
		// For the entire vertical height, insert a pixel in either the beginning of end of the row.
		for (auto i = 0; i < size_.y; ++i) {
			// E.g. pixels_.begin() + 1 * (size.x + 1) + 0
			// Will add a pixel in the beginning of the second row.
			auto index = i * (size_.x + 1) + offset;
			pixels_.insert(pixels_.begin() + static_cast<std::size_t>(index), color);
		}
		// Increment size.
		size_.x += 1;
	} else if (side.IsVertical()) {
		// Create row of the given color and same horizontal size.
		std::vector<Color> row(size_.x, color);
		std::vector<Color>::iterator iterator;
		if (side.IsTop()) {
			// Offset relative position.
			position_.y -= 1;
			// Add to beginning of pixels vector.
			iterator = pixels_.begin();
		} else if (side.IsBottom()) {
			// Add to end of pixels vector.
			iterator = pixels_.end();
		}
		// Copy row vector into pixels vector.
		pixels_.insert(iterator, row.begin(), row.end());
		// Increment size.
		size_.y += 1;
	}
}
*/
void Image::SetPixel(const V2_int& position, const Color& color) {
	auto index = position.y * size_.x + position.x;
	assert(pixels_.size() > 0 && 
		position.x < size_.x && 
		position.y < size_.y && 
		index < pixels_.size() &&
		"Pixel out of range of image size");
	pixels_[index] = color;
}

std::ostream& operator<<(std::ostream& os, Image& image) {
	for (std::size_t i{ 0 }; i < image.pixels_.size(); ++i) {
		// Add newline every time the vector loops over the size.x length.
		if (i % image.size_.x == 0 && i) {
			os << std::endl;
		}
		const auto& color = image.pixels_[i];
		if (!color.IsTransparent()) {
			os << "#";
		} else {
			os << " ";
		}
	}
	return os;
}


} // namespace engine