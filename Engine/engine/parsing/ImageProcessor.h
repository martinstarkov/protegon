#pragma once

#include "utils/Vector2.h"

#include "renderer/Color.h"

#include <vector>

namespace engine {

class Image {
public:
	Image(const char* path);
	Color GetPixel(V2_int position) const;
	V2_int GetSize() const;
	friend std::ostream& operator<<(std::ostream& os, Image& image);
private:
	void SetPixel(V2_int position, const Color& color);
	V2_int size;
	std::vector<Color> pixels;
};

class ImageProcessor {
public:
	// Returns a vector of images and their position within the outer image
	static std::vector<std::pair<Image, V2_int>> GetImages(const char* image_path);
private:
};

} // namespace engine