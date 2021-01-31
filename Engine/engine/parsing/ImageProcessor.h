#pragma once

#include "math/Vector2.h"

#include "renderer/Color.h"
#include "parsing/Image.h"

#include <vector>
#include <utility>

namespace engine {

class ImageProcessor {
public:
	// Returns a vector of transparent column disconnected images and their positions within the outer image.
	static std::vector<Image> GetDisconnectedImages(const char* image_path);
	// Trims / expands each image relative to the center of the given axis to fit the hitbox.
	//static std::vector<Image>& CenterOnHitbox(std::vector<Image>& images, V2_int hitbox_size = V2_int{ 0, 0 }, Side side = Direction::BOTTOM);
private:
	// Return the coordinates of the top left and bottom right most non-transparent corner pixels in an image.
	// I.e. Remove all transparent pixels from around an image and this function will return { { 0, 0 }, { size.x, size.y } }.
	static std::pair<V2_int, V2_int> GetCorners(const Image& image);
};

} // namespace engine