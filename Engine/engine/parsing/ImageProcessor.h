#pragma once

#include "utils/Vector2.h"

#include "renderer/Color.h"

#include <vector>
#include <utility>

namespace engine {

class ImageProcessor;

class Image {
public:
	// Analyze png file to generate image.
	Image(const char* path);
	// Return the color at a given x,y coordinate.
	Color GetPixel(V2_int position) const;
	// Returns the size of the image in pixels.
	V2_int GetSize() const;
	// Returns an image within the image with the given edge coordinates.
	// Obligatory comment on imageception.
	Image GetSubImage(V2_int top_left, V2_int bottom_right) const;
	// Prints image out in the console.
	// Non-transparent pixels are hashtags while transparent ones as spaces.
	friend std::ostream& operator<<(std::ostream& os, Image& image);
	friend ImageProcessor;
private:
	// For creating sub images.
	Image(std::vector<Color> pixels, V2_int size);
	// Set a given pixel position to a color, used for populating pixels vector initially.
	void SetPixel(V2_int position, const Color& color);
	V2_int size_;
	// 1D array of 2D points in the format: y * size.x + x
	std::vector<Color> pixels_;
};

class ImageProcessor {
public:
	// Returns a vector of transparent column disconnected images and their positions within the outer image.
	static std::vector<std::pair<Image, V2_int>> GetDisconnectedImages(const char* image_path);
private:
	// Return the coordinates of the top left and bottom right most non-transparent corner pixels in an image.
	// I.e. Remove all transparent pixels from around an image and this function will return { { 0, 0 }, { size.x, size.y } }.
	static std::pair<V2_int, V2_int> GetCorners(const Image& image);
};

} // namespace engine