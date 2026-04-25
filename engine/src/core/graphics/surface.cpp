#include "core/graphics/surface.h"

#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "core/util/file.h"

namespace ptgn::impl {

Surface::Surface(
	V2_int size, std::span<const std::uint8_t> pixels, int channels, bool flip_vertically
) :
	channels_{ channels }, size_{ size } {
	PTGN_ASSERT(channels > 0 && channels <= 4, "Invalid channel count: ", channels);
	PTGN_ASSERT(size.x > 0 && size.y > 0, "Invalid surface size: ", size);
	PTGN_ASSERT(
		pixels.size() == static_cast<std::size_t>(size.x) * size.y * channels,
		"Pixel data size does not match expected size for given surface dimensions"
	);
	if (!flip_vertically) {
		pixels_ = std::vector<std::uint8_t>(pixels.begin(), pixels.end());
	} else {
		auto row_bytes{ static_cast<std::size_t>(size.x) * static_cast<std::size_t>(channels) };

		pixels_.resize(pixels.size());

		for (std::size_t dst_row{ 0 }; dst_row < static_cast<std::size_t>(size.y); ++dst_row) {
			std::size_t src_row{ static_cast<std::size_t>(size.y) - dst_row - 1 };

			std::copy_n(
				pixels.begin() + static_cast<std::ptrdiff_t>(src_row * row_bytes), row_bytes,
				pixels_.begin() + static_cast<std::ptrdiff_t>(dst_row * row_bytes)
			);
		}
	}
}

Surface::Surface(const path& filepath, int desired_channels) {
	PTGN_ASSERT(
		FileExists(filepath),
		"Cannot create surface from a nonexistent filepath: ", filepath.string()
	);

	int width{ 0 };
	int height{ 0 };
	int channels_in_file{ 0 };

	auto abs_path{ GetAbsolutePath(filepath) };

	auto data =
		stbi_load(abs_path.string().c_str(), &width, &height, &channels_in_file, desired_channels);

	channels_ = desired_channels;

	PTGN_ASSERT(
		data != nullptr, "Failed to load image '", filepath.string(), "': ", stbi_failure_reason()
	);

	PTGN_ASSERT(width > 0 && height > 0, "Loaded image has invalid size");

	size_ = { width, height };

	std::size_t total_bytes{ static_cast<std::size_t>(width) * height * channels_ };

	pixels_.resize(total_bytes);
	std::memcpy(pixels_.data(), data, total_bytes);

	stbi_image_free(data);
}

void Surface::FlipVertically() {
	PTGN_ASSERT(!IsEmpty(), "Cannot vertically flip an empty surface");

	const std::size_t row_bytes = static_cast<std::size_t>(size_.x) * channels_;

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

	PTGN_ASSERT(channels_ == 4, "GetPixel only works for surfaces with 4 channels");

	const std::size_t byte_index = pixel_index * channels_;
	PTGN_ASSERT(byte_index + 3 < pixels_.size(), "Pixel index outside of range of surface");

	return { pixels_[byte_index + 0], pixels_[byte_index + 1], pixels_[byte_index + 2],
			 pixels_[byte_index + 3] };
}

int Surface::GetChannelCount() const {
	return channels_;
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

std::expected<void, std::string> Surface::SavePNG(const path& filepath) const {
#ifdef __EMSCRIPTEN__
	return std::unexpected(
		"Saving PNGs is not supported in Emscripten builds. This function should not be called."
	);
#endif

	auto stride_in_bytes{ size_.x * channels_ };

	auto success{ stbi_write_png(
		filepath.string().c_str(), size_.x, size_.y, channels_, pixels_.data(), stride_in_bytes
	) };

	if (!success) {
		return std::unexpected("Failed to save PNG");
	}

	return {};
}

} // namespace ptgn::impl