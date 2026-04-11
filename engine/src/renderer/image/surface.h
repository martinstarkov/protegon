#pragma once

#include <cstdint>
#include <vector>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/file.h"
#include "renderer/primitives/color.h"

namespace ptgn {

class FontSystem;

namespace impl {

class Surface {
public:
	explicit Surface(const path& filepath);

	/// @brief Mirrors the surface vertically.
	void FlipVertically();

	/// @param coordinate Pixel coordinate from [0, size).
	/// @return Color value of the given pixel.
	Color GetPixel(V2_int coordinate) const;

	/// @brief Calls the given function for each pixel in the surface in row-major order.
	/// @param func The function must be callable as void(V2_int, Color).
	template <InvocableR<void, V2_int, Color> F>
	void ForEachPixel(F&& func) const {
		PTGN_ASSERT(!pixels_.empty(), "Cannot loop through each pixel of an empty surface");
		for (int j{ 0 }; j < size_.y; j++) {
			auto row_index{ static_cast<std::size_t>(j) * static_cast<std::size_t>(size_.x) };
			for (int i{ 0 }; i < size_.x; i++) {
				V2_int coordinate{ i, j };
				auto index{ row_index + static_cast<std::size_t>(i) };
				auto pixel{ GetPixel(index) };
				std::invoke(std::forward<F>(func), coordinate, pixel);
			}
		}
	}

	V2_int GetSize() const;

	[[nodiscard]] const std::uint8_t* Data() const;

	[[nodiscard]] bool IsEmpty() const;

private:
	friend class ptgn::FontSystem;

	/// @param pixel_index One dimensionalized index into the data array.
	Color GetPixel(std::size_t pixel_index) const;

	/// @brief Surface pixel data is currently always stored as RGBA32.
	static constexpr std::size_t kBytesPerPixel{ 4 };

	/// @brief The row major one dimensionalized array of pixel values that makes up the surface.
	std::vector<std::uint8_t> pixels_;

	V2_int size_;
};

} // namespace impl

} // namespace ptgn