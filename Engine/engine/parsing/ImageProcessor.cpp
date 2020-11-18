#include "ImageProcessor.h"

#include <SDL_image.h>
#include <SDL.h>

#include <cstdint>

namespace engine {

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
	SDL_Surface* image = SDL_ConvertSurfaceFormat(temp_image, SDL_PIXELFORMAT_RGBA8888, 0);
	SDL_FreeSurface(temp_image);
	assert(image != nullptr && "Failed to convert image to RGBA format");
	size.x = image->w;
	size.y = image->h;
	pixels.resize(static_cast<std::size_t>(size.x) * static_cast<std::size_t>(size.y), Color{});
	for (auto j = 0; j < size.y; ++j) {
		for (auto i = 0; i < size.x; ++i) {
			SetPixel({ i, j }, GetSurfacePixelColor(image, { i, j }));
		}
	}
	SDL_FreeSurface(image);
}

Color Image::GetPixel(V2_int position) const { 
	auto index = position.y * size.x + position.x;
	assert(pixels.size() > 0 && position.x < size.x && position.y < size.y&& index < pixels.size() && "Pixel out of range of image size");
	return pixels[index];
}

V2_int Image::GetSize() const { return size; }

void Image::SetPixel(V2_int position, const Color& color) {
	auto index = position.y * size.x + position.x;
	assert(pixels.size() > 0 && position.x < size.x && position.y < size.y&& index < pixels.size() && "Pixel out of range of image size");
	pixels[index] = color;
}

std::ostream& operator<<(std::ostream& os, Image& image) {
	for (auto i = 0; i < image.pixels.size(); ++i) {
		// Add newline every time the vector loops over the size.x length.
		if (i % image.size.x == 0) {
			os << std::endl;
		}
		const auto& color = image.pixels[i];
		os << color;
	}
	return os;
}

std::vector<std::pair<Image, V2_int>> ImageProcessor::GetImages(const char* image_path) {
	V2_int size, position;
	auto full_image = Image{ image_path };
	//LOG(full_image);
	return {};
}

} // namespace engine