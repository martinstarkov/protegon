#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/file.h"

namespace ptgn {

class FontSystem;

namespace impl {

class Surface {
public:
	Surface(
		V2_int size, std::span<const std::uint8_t> pixels, std::uint8_t channels = 4,
		bool flip_vertically = false
	);

	explicit Surface(std::span<const std::uint8_t> bytes, std::uint8_t desired_channels = 4);
	explicit Surface(std::span<const std::byte> bytes, std::uint8_t desired_channels = 4);

	explicit Surface(const path& file, std::uint8_t desired_channels = 4);

	/// @brief Mirrors the surface vertically.
	void FlipVertically();

	/// @param coordinate Pixel coordinate clamped to [0, size).
	/// @return Color value of the given pixel.
	Color GetPixel(V2_int coordinate) const;

	/// @brief Calls the given function for each pixel in the surface in row-major order.
	/// @param fn The function must be callable as void(V2_int, Color).
	void ForEachPixel(InvocableR<void, V2_int, Color> auto fn) const {
		PTGN_ASSERT(!pixels_.empty(), "Cannot loop through each pixel of an empty surface");
		for (int j{ 0 }; j < size_.y; ++j) {
			auto row_index{ j * size_.x };
			for (int i{ 0 }; i < size_.x; ++i) {
				V2_int coordinate{ i, j };
				auto index{ row_index + i };
				PTGN_ASSERT(index >= 0);
				auto pixel{ GetPixel(static_cast<std::size_t>(index)) };
				std::invoke(fn, coordinate, pixel);
			}
		}
	}

	int GetChannelCount() const;

	V2_int GetSize() const;

	[[nodiscard]] const std::uint8_t* Data() const;

	[[nodiscard]] bool IsEmpty() const;

	/// @brief Encodes the surface pixel data as a PNG file in memory.
	std::vector<std::byte> EncodePNG() const;

	[[nodiscard("Check if png save succeeded")]] std::expected<void, FileWriteError> SavePNG(
		const path& file
	) const;

private:
	friend class ptgn::FontSystem;

	/// @param pixel_index One dimensionalized index into the data array clamped to pixels.size()-3
	/// (for a 4 channel surface)
	Color GetPixel(std::size_t pixel_index) const;

	/// @brief Number of channels in the pixel data.
	std::uint8_t channels_{ 4 };

	/// @brief The row major one dimensionalized array of pixel values that makes up the surface.
	std::vector<std::uint8_t> pixels_;

	V2_int size_;
};

} // namespace impl

} // namespace ptgn