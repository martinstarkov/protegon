#pragma once

#include "math/Vector2.h"

#include "renderer/Color.h"

#include <vector>
#include <utility>

// TODO: Add transparent column function for a given edge.

namespace engine {

class ImageProcessor;

class Image {
public:
	// Analyze png file to generate image.
	Image(const char* path);
	// Return the color at a given x,y coordinate.
	Color GetPixel(const V2_int& position) const;
	// Returns the size of the image in pixels.
	V2_int GetSize() const;
	// Returns the original size of the image in pixels before any fitting was done.
	V2_int GetOriginalSize() const;
	// Returns the position of the image relative to the outer most image.
	V2_int GetPosition() const;
	// Returns an image within the image with the given edge coordinates.
	// Obligatory comment on imageception.
	Image GetSubImage(const V2_int& top_left, const V2_int& bottom_right) const;
	// Add row / column of pixels of the given color to the pixels array.
	//void AddSide(Side side, Color color = TRANSPARENT);
	// Prints image out in the console.
	// Non-transparent pixels are hashtags while transparent ones as spaces.
	friend std::ostream& operator<<(std::ostream& os, Image& image);
	friend ImageProcessor;
private:
	// For creating sub images.
	Image(const std::vector<Color>& pixels, const V2_int& size, const V2_int& relative_position);
	// Set a given pixel position to a color, used for populating pixels vector initially.
	void SetPixel(const V2_int& position, const Color& color);
	V2_int size_;
	V2_int original_size_;
	// Position within the outer most image.
	V2_int position_{ 0, 0 };
	// 1D array of 2D points in the format: y * size.x + x
	std::vector<Color> pixels_;
};

} // namespace engine