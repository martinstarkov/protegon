#include "core/graphics/surface.h"

#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <ranges>
#include <span>
#include <string>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "core/util/file.h"

namespace ptgn::impl {

namespace {

void WritePngBytesToVector(void* context, void* data, int size) {
	PTGN_ASSERT(context, "PNG output context is null");
	PTGN_ASSERT(data, "PNG output data is null");
	PTGN_ASSERT(size >= 0, "PNG output size cannot be negative");

	auto& bytes{ *static_cast<std::vector<std::byte>*>(context) };

	auto* first{ static_cast<std::byte*>(data) };
	bytes.insert(bytes.end(), first, first + size);
}

} // namespace

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
		pixels_ = std::ranges::to<std::vector<std::uint8_t>>(pixels);
	} else {
		auto row_bytes{ static_cast<std::size_t>(size.x) * channels };

		pixels_.resize(pixels.size());

		for (auto dst_row{ 0uz }; dst_row < static_cast<std::size_t>(size.y); ++dst_row) {
			auto src_row{ size.y - dst_row - 1uz };

			std::copy_n(
				pixels.begin() + static_cast<std::ptrdiff_t>(src_row * row_bytes), row_bytes,
				pixels_.begin() + static_cast<std::ptrdiff_t>(dst_row * row_bytes)
			);
		}
	}
}

Surface::Surface(std::span<const std::byte> bytes, int desired_channels) {
	PTGN_ASSERT(bytes.data(), "Embedded PNG binary is null");
	PTGN_ASSERT(bytes.size() > 0, "Embedded PNG binary is empty");

	int source_channel_count{ 0 };

	auto data{ stbi_load_from_memory(
		reinterpret_cast<const unsigned char*>(bytes.data()), // NOSONAR
		static_cast<int>(bytes.size()), &size_.x, &size_.y, &source_channel_count, desired_channels
	) };

	PTGN_ASSERT(data, "Failed to load image from memory: ", stbi_failure_reason());

	PTGN_ASSERT(size_.IsPositive(), "Loaded image has invalid size");

	auto total_bytes{ static_cast<std::size_t>(size_.x) * size_.y * channels_ };

	pixels_.resize(total_bytes);
	std::memcpy(pixels_.data(), data, total_bytes);

	stbi_image_free(data);
}

Surface::Surface(const path& file, int desired_channels) : channels_{ desired_channels } {
	PTGN_ASSERT(FileExists(file), "Cannot create surface from a nonexistent file: ", file.string());

	int channels_in_file{ 0 };

	auto abs_path{ GetAbsolutePath(file) };

	auto data{ stbi_load(
		abs_path.string().c_str(), &size_.x, &size_.y, &channels_in_file, desired_channels
	) };

	PTGN_ASSERT(data, "Failed to load image '", file.string(), "': ", stbi_failure_reason());

	PTGN_ASSERT(size_.IsPositive(), "Loaded image has invalid size");

	auto total_bytes{ static_cast<std::size_t>(size_.x) * size_.y * channels_ };

	pixels_.resize(total_bytes);
	std::memcpy(pixels_.data(), data, total_bytes);

	stbi_image_free(data);
}

void Surface::FlipVertically() {
	PTGN_ASSERT(!IsEmpty(), "Cannot vertically flip an empty surface");

	auto row_bytes{ static_cast<std::size_t>(size_.x) * channels_ };

	for (auto row{ 0uz }; row < size_.y / 2uz; ++row) {
		auto top_begin{ pixels_.begin() + static_cast<std::ptrdiff_t>(row * row_bytes) };
		auto top_end{ top_begin + static_cast<std::ptrdiff_t>(row_bytes) };
		auto bot_begin{ pixels_.begin() +
						static_cast<std::ptrdiff_t>((size_.y - row - 1uz) * row_bytes) };

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

	auto pixel_index{ static_cast<std::size_t>(coordinate.y) * size_.x + coordinate.x };

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

std::vector<std::byte> Surface::EncodePNG() const {
	std::vector<std::byte> encoded;

	auto success{ stbi_write_png_to_func(
		&WritePngBytesToVector, &encoded, size_.x, size_.y, channels_, pixels_.data(),
		size_.x * channels_
	) };

	PTGN_ASSERT(success, "Failed to encode surface as PNG");
	PTGN_ASSERT(!encoded.empty(), "Encoded PNG was empty");

	return encoded;
}

std::expected<void, FileWriteError> Surface::SavePNG(const path& file) const {
	auto png_bytes{ EncodePNG() };
	PTGN_ASSERT(
		HasExtension(file, ".png"),
		"File extension must be .png to save surface as PNG: ", file.string()
	);
	return WriteBinary(file, png_bytes);
}

} // namespace ptgn::impl