#include "ImageProcessor.h"

#include <cstdint>
#include <algorithm>

namespace engine {

std::vector<Image> ImageProcessor::GetDisconnectedImages(const char* image_path) {
	std::vector<Image> images;
	auto full_image = Image{ image_path };
	auto [top_left, bottom_right] = GetCorners(full_image);
	auto outer_sub_image = full_image.GetSubImage(top_left, bottom_right);
	V2_int min{ outer_sub_image.size_ };
	V2_int max{ 0, 0 };
	for (auto column = 0; column < outer_sub_image.size_.x; ++column) {
		// Search for transparent rows.
		auto transparent_pixels = 0;
		for (auto row = 0; row < outer_sub_image.size_.y; ++row) {
			const auto& color = outer_sub_image.GetPixel({ column, row });
			// If any pixel in the row is not transparent, break.
			if (!color.IsTransparent()) {
				min.y = std::min(min.y, row);
				max.y = std::max(max.y, row);
				min.x = std::min(min.x, column);
				max.x = std::max(max.x, column);
			} else {
				++transparent_pixels;
			}
		}
		// Column is transparent or is the final column
		if (transparent_pixels == outer_sub_image.size_.y || column == outer_sub_image.size_.x - 1) {
			// This means that if the previous column was transparent, skip this column.
			// (since min.x is reset every time a column is transparent).
			if (min.x != outer_sub_image.size_.x) {
				auto sub_image = outer_sub_image.GetSubImage(min, max);
				// Offset by how much the outer_sub_image is within the full image.
				sub_image.position_ += top_left;
				// Add sub image to vector.
				images.emplace_back(sub_image);
			}
			// Reset local pixel min / max so the next transparent column isn't considered and new image outlines are found.
			min = outer_sub_image.size_;
			max = { 0, 0 };
		}
	}
	return std::move(images);
}
/*
std::vector<Image>& ImageProcessor::CenterOnHitbox(std::vector<Image>& images, V2_int hitbox_size, Side side) {
	V2_int size = hitbox_size;
	V2_int largest_size;
	for (auto& image : images) {
		largest_size.x = std::max(largest_size.x, image.size_.x);
		largest_size.y = std::max(largest_size.y, image.size_.y);
	}
	if (size.IsZero()) {
		size = largest_size;
	}
	
	// TODO: In the future make it so the hitbox selection is smart based on the side you select.
	// else if (side.IsVertical()) {
	//	size.y = largest_size.y;
	//} else if (side.IsHorizontal()) {
	//	size.x = largest_size.x;
	//}
	
	//LOG("Hitbox size: " << size);
	for (auto& image : images) {
		V2_int difference = size - image.size_;
		//LOG("---------------------------------");
		//LOG("original size: " << image.size_);
		//LOG("size difference: " << difference);
		if (!side) {
			// TODO: Add individual point centering here.
		} else {
			int additions = 0;
			auto [perpendicular_1, perpendicular_2] = side.GetPerpendicular();
			// By default add differences to X-directions.
			int& diff = difference.x;
			if (side.IsVertical()) {
				if (difference.y > 0) {
					additions = difference.y;
				}
				diff = difference.x;
			} else if (side.IsHorizontal()) {
				if (difference.x > 0) {
					additions = difference.x;
				}
				diff = difference.y;
			}
			for (; diff > 0; --diff) {
				if (diff % 2 == 0) { // Even difference.
					image.AddSide(perpendicular_2);
				} else if (diff % 2 == 1) { // Odd difference.
					image.AddSide(perpendicular_1);
				}
			}
			for (auto i = 0; i < additions; ++i) {
				image.AddSide(side.GetOpposite());
			}
		}
		//LOG(image);
		//LOG("size: " << image.size_);
		//LOG("---------------------------------");
	}
	return images;
}
*/
std::pair<V2_int, V2_int> ImageProcessor::GetCorners(const Image& image) {
	// Min and max are initially the extremes, so all pixels will be considered initially.
	V2_int min{ image.size_ };
	V2_int max{ 0, 0 };
	// Cycles through each pixel and finds the min and max x and y coordinates that aren't transparent.
	for (std::size_t i{ 0 }; i < image.pixels_.size(); ++i) {
		const auto& color = image.pixels_[i];
		if (!color.IsTransparent()) {
			// Coordinates of the pixel.
			int x = i % image.size_.x;
			int y = engine::math::Round(i / image.size_.x);
			// Find min and max (top left corner of the image and bottom right).
			min.x = std::min(min.x, x);
			min.y = std::min(min.y, y);
			max.x = std::max(max.x, x);
			max.y = std::max(max.y, y);
		}
	}
	return { min, max };
}

} // namespace engine