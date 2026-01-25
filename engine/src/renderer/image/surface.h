#pragma once

#include <vector>

#include "core/util/file.h"
#include "math/vector2.h"
#include "renderer/api/color.h"

namespace ptgn::impl {

struct Surface {
	explicit Surface(const path& filepath);

	// Mirrors the surface vertically.
	void FlipVertically();

	// @param coordinate Pixel coordinate from [0, size).
	// @return Color value of the given pixel.
	[[nodiscard]] Color GetPixel(V2_int coordinate) const;

	// @param callback Function to be called for each pixel.
	void ForEachPixel(const std::function<void(V2_int, Color)>& function) const;

	// Surface pixel data is currently always stored as RGBA32.
	static constexpr std::size_t bytes_per_pixel{ 4 };
	// The row major one dimensionalized array of pixel values that makes up the surface.
	std::vector<std::uint8_t> pixels;

	V2_int size;

private:
	// @param index One dimensionalized index into the data array.
	[[nodiscard]] Color GetPixel(std::size_t index) const;
};

} // namespace ptgn::impl