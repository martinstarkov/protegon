#include "renderer/image/surface.h"

#include <stb_image.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "core/graphics/color.h"

namespace ptgn::impl {

Surface::Surface(const path& filepath) {
	PTGN_ASSERT(
		FileExists(filepath),
		"Cannot create surface from a nonexistent filepath: ", filepath.string()
	);

	int width{ 0 };
	int height{ 0 };
	int channels_in_file{ 0 };

	auto abs_path{ GetAbsolutePath(filepath) };

	auto data = stbi_load(
		abs_path.string().c_str(), &width, &height, &channels_in_file,
		static_cast<int>(kBytesPerPixel)
	);

	PTGN_ASSERT(
		data != nullptr, "Failed to load image '", filepath.string(), "': ", stbi_failure_reason()
	);

	PTGN_ASSERT(width > 0 && height > 0, "Loaded image has invalid size");

	size_ = { width, height };

	const std::size_t total_bytes =
		static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kBytesPerPixel;

	pixels_.resize(total_bytes);
	std::memcpy(pixels_.data(), data, total_bytes);

	stbi_image_free(data);
}

void Surface::FlipVertically() {
	PTGN_ASSERT(!IsEmpty(), "Cannot vertically flip an empty surface");

	const std::size_t row_bytes = static_cast<std::size_t>(size_.x) * kBytesPerPixel;

	for (std::size_t row = 0; row < static_cast<std::size_t>(size_.y) / 2; ++row) {
		auto top_begin = pixels_.begin() + static_cast<std::ptrdiff_t>(row * row_bytes);
		auto top_end   = top_begin + static_cast<std::ptrdiff_t>(row_bytes);
		auto bot_begin =
			pixels_.begin() +
			static_cast<std::ptrdiff_t>((static_cast<std::size_t>(size_.y) - row - 1) * row_bytes);

		std::swap_ranges(top_begin, top_end, bot_begin);
	}
}

Color Surface::GetPixel(V2_int coordinate) const {
	PTGN_ASSERT(
		coordinate.x >= 0 && coordinate.x < size_.x, "X Coordinate '", coordinate.x,
		"' outside of surface width: ", size_.x
	);
	PTGN_ASSERT(
		coordinate.y >= 0 && coordinate.y < size_.y, "Y Coordinate '", coordinate.y,
		"' outside of surface height: ", size_.y
	);

	const auto pixel_index =
		static_cast<std::size_t>(coordinate.y) * static_cast<std::size_t>(size_.x) +
		static_cast<std::size_t>(coordinate.x);

	return GetPixel(pixel_index);
}

Color Surface::GetPixel(std::size_t pixel_index) const {
	PTGN_ASSERT(!IsEmpty(), "Cannot get pixel of an empty surface");

	const std::size_t byte_index = pixel_index * kBytesPerPixel;
	PTGN_ASSERT(byte_index + 3 < pixels_.size(), "Pixel index outside of range of surface");

	return { pixels_[byte_index + 0], pixels_[byte_index + 1], pixels_[byte_index + 2],
			 pixels_[byte_index + 3] };
}

V2_int Surface::GetSize() const {
	return size_;
}

const std::uint8_t* Surface::Data() const {
	return pixels_.data();
}

[[nodiscard]] bool Surface::IsEmpty() const {
	return pixels_.empty();
}

} // namespace ptgn::impl