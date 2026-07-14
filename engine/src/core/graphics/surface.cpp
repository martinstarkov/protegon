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
	V2_int size, std::span<const std::uint8_t> pixels, std::uint8_t channels, bool flip_vertically
) :
	channels_{ channels }, size_{ size } {
	PTGN_ASSERT(channels <= 4, "Invalid channel count: ", channels);
	PTGN_ASSERT(size.IsPositive(), "Invalid surface size: ", size);
	PTGN_ASSERT(
		pixels.size() ==
			static_cast<std::size_t>(size.x) * static_cast<std::size_t>(size.y) * channels,
		"Pixel data size does not match expected size for given surface dimensions"
	);
	if (!flip_vertically) {
		pixels_ = std::ranges::to<std::vector<std::uint8_t>>(pixels);
	} else {
		auto row_bytes{ static_cast<std::size_t>(size.x) * channels };

		pixels_.resize(pixels.size());

		for (auto dst_row{ 0uz }; dst_row < static_cast<std::size_t>(size.y); ++dst_row) {
			auto src_row{ static_cast<std::size_t>(size.y) - dst_row - 1uz };

			std::copy_n(
				pixels.begin() + static_cast<std::ptrdiff_t>(src_row * row_bytes), row_bytes,
				pixels_.begin() + static_cast<std::ptrdiff_t>(dst_row * row_bytes)
			);
		}
	}
}

Surface::Surface(std::span<const std::byte> bytes, std::uint8_t desired_channels) :
	Surface{ { reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size() },
			 desired_channels } {
	static_assert(std::is_same_v<std::uint8_t, unsigned char>);
}

Surface::Surface(std::span<const std::uint8_t> bytes, std::uint8_t desired_channels) {
	PTGN_ASSERT(bytes.data(), "Embedded PNG binary is null");
	PTGN_ASSERT(bytes.size() > 0, "Embedded PNG binary is empty");

	int source_channel_count{ 0 };

	auto data{ stbi_load_from_memory(
		bytes.data(), // NOSONAR
		static_cast<int>(bytes.size()), &size_.x, &size_.y, &source_channel_count, desired_channels
	) };

	PTGN_ASSERT(data, "Failed to load image from memory: ", stbi_failure_reason());

	PTGN_ASSERT(size_.IsPositive(), "Loaded image has invalid size: ", size_);

	auto total_bytes{ static_cast<std::size_t>(size_.x) * static_cast<std::size_t>(size_.y) *
					  channels_ };

	pixels_.resize(total_bytes);
	std::memcpy(pixels_.data(), data, total_bytes);

	stbi_image_free(data);
}

Surface::Surface(const path& file, std::uint8_t desired_channels) : channels_{ desired_channels } {
	PTGN_ASSERT(FileExists(file), "Cannot create surface from a nonexistent file: ", file.string());

	int channels_in_file{ 0 };

	auto abs_path{ GetAbsolutePath(file) };

	auto data{ stbi_load(
		abs_path.string().c_str(), &size_.x, &size_.y, &channels_in_file, desired_channels
	) };

	PTGN_ASSERT(data, "Failed to load image '", file.string(), "': ", stbi_failure_reason());

	PTGN_ASSERT(size_.IsPositive(), "Loaded image has invalid size: ", size_);

	auto total_bytes{ static_cast<std::size_t>(size_.x) * static_cast<std::size_t>(size_.y) *
					  channels_ };

	pixels_.resize(total_bytes);
	std::memcpy(pixels_.data(), data, total_bytes);

	stbi_image_free(data);
}

void Surface::FlipVertically() {
	PTGN_ASSERT(!IsEmpty(), "Cannot vertically flip an empty surface");
	PTGN_ASSERT(size_.IsPositive(), "Surface size is invalid: ", size_);

	auto row_bytes{ static_cast<std::size_t>(size_.x) * channels_ };

	for (auto row{ 0uz }; row < static_cast<std::size_t>(size_.y) / 2uz; ++row) {
		auto top_begin{ pixels_.begin() + static_cast<std::ptrdiff_t>(row * row_bytes) };
		auto top_end{ top_begin + static_cast<std::ptrdiff_t>(row_bytes) };
		auto bot_begin{
			pixels_.begin() +
			static_cast<std::ptrdiff_t>((static_cast<std::size_t>(size_.y) - row - 1uz) * row_bytes)
		};

		std::swap_ranges(top_begin, top_end, bot_begin);
	}
}

Color Surface::GetPixel(V2_int coordinate) const {
	PTGN_ASSERT(size_.IsPositive(), "Surface size is invalid: ", size_);
	coordinate = Clamp(coordinate, {}, size_);

	auto pixel_index{ static_cast<std::size_t>(coordinate.y) * static_cast<std::size_t>(size_.x) +
					  static_cast<std::size_t>(coordinate.x) };

	return GetPixel(pixel_index);
}

Color Surface::GetPixel(std::size_t pixel_index) const {
	PTGN_ASSERT(!IsEmpty(), "Cannot get pixel of an empty surface");

	PTGN_ASSERT(channels_ == 4, "GetPixel only works for surfaces with 4 channels");

	std::size_t byte_index{ pixel_index * channels_ };

	byte_index = std::max(byte_index, pixels_.size() - 3);

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