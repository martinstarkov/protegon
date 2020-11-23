#include "Image.h"

#include <SDL_image.h>
#include <SDL.h>
#include <iterator>
#include <cstdint>
#include <cassert>

namespace engine {

// Convert an SDL surface coordinate to a 4 byte integer value containg the RGBA32 color of the pixel.
static std::uint32_t GetSurfacePixelColor(SDL_Surface* image, V2_int position) {
	// Source: http://sdl.beuc.net/sdl.wiki/Pixel_Access
	auto bpp = image->format->BytesPerPixel;
	// Here p is the address to the pixel we want to retrieve.
	std::uint8_t* p = static_cast<std::uint8_t*>(image->pixels) + static_cast<std::size_t>(position.y) * static_cast<std::size_t>(image->pitch) + static_cast<std::size_t>(position.x) * static_cast<std::size_t>(bpp);
	switch (bpp) {
		case 1:
			return *p;
			break;
		case 2:
			return *(Uint16*)p;
			break;
		case 3:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
				return p[0] << 16 | p[1] << 8 | p[2];
			else
				return p[0] | p[1] << 8 | p[2] << 16;
			break;
		case 4:
			return *(Uint32*)p;
			break;
		default:
			return 0; // Shouldn't happen, but avoids warnings.
	}
}

Image::Image(const char* path) {
	// TODO: Add support for formats other than PNG.
	SDL_Surface* temp_image = IMG_Load(path);
	if (!temp_image) {
		printf("IMG_Load: %s\n", IMG_GetError());
		// handle error
		assert(false && "Failed to retrieve image data");
	}
	SDL_Surface* image = SDL_ConvertSurfaceFormat(temp_image, SDL_PIXELFORMAT_RGBA32, 0);
	SDL_FreeSurface(temp_image);
	assert(image != nullptr && "Failed to convert image to RGBA format");
	size_.x = image->w;
	size_.y = image->h;
	pixels_.resize(static_cast<std::size_t>(size_.x) * static_cast<std::size_t>(size_.y), Color{});
	for (auto j = 0; j < size_.y; ++j) {
		for (auto i = 0; i < size_.x; ++i) {
			SetPixel({ i, j }, GetSurfacePixelColor(image, { i, j }));
		}
	}
	SDL_FreeSurface(image);
}

Image::Image(std::vector<Color> pixels, V2_int size, V2_int relative_position) : pixels_{ std::move(pixels) }, size_{ size }, original_size_{ size }, position_{ relative_position } {}

Color Image::GetPixel(V2_int position) const {
	auto index = position.y * size_.x + position.x;
	assert(pixels_.size() > 0 && position.x < size_.x&& position.y < size_.y&& index < pixels_.size() && "Pixel out of range of image size");
	return pixels_[index];
}

V2_int Image::GetSize() const { return size_; }
V2_int Image::GetOriginalSize() const { return original_size_; }

V2_int Image::GetPosition() const { return position_; }

Image Image::GetSubImage(V2_int top_left, V2_int bottom_right) const {
	std::vector<Color> sub_pixels;
	// Add { 1, 1 } since taking difference omits one row and column of pixels.
	V2_int sub_image_size = bottom_right - top_left + V2_int{ 1, 1 };
	auto sub_pixels_size = sub_image_size.x * sub_image_size.y;
	sub_pixels.reserve(sub_pixels_size);
	// Find indexes of first and last points to cut down on loop time.
	auto first_index = top_left.y * size_.x + top_left.x;
	auto last_index = bottom_right.y * size_.x + bottom_right.x;
	assert(first_index >= 0 && first_index < pixels_.size() && "Top left coordinate out of range of image pixels");
	assert(last_index >= 0 && last_index < pixels_.size() && "Top right coordinate out of range of image pixels");
	for (auto i = first_index; i < last_index + 1; ++i) {
		auto mod_index = i % size_.x;
		if (mod_index >= top_left.x && mod_index <= bottom_right.x) {
			sub_pixels.push_back(pixels_[i]);
		}
	}
	return Image{ std::move(sub_pixels), sub_image_size, top_left };
}

void Image::AddSide(Side side, Color color) {
	if (side.IsHorizontal()) {
		// Offset relative to the beginning of the pixels vector.
		int offset;
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
			pixels_.insert(pixels_.begin() + i * (size_.x + 1) + offset, color);
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

void Image::SetPixel(V2_int position, const Color& color) {
	auto index = position.y * size_.x + position.x;
	assert(pixels_.size() > 0 && position.x < size_.x&& position.y < size_.y&& index < pixels_.size() && "Pixel out of range of image size");
	pixels_[index] = color;
}

std::ostream& operator<<(std::ostream& os, Image& image) {
	for (auto i = 0; i < image.pixels_.size(); ++i) {
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