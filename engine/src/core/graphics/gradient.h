#pragma once

#include <string>
#include <vector>

#include "core/graphics/color.h"

namespace ptgn {

namespace impl {

struct ColorStop {
	/// @brief Range: [0, 1]
	float t{ 0.5f };
	Color color;
};

} // namespace impl

class Gradient {
public:
	Gradient() = default;
	explicit Gradient(const std::string& css);

	/// @brief Evaluates the gradient at a given position t in [0, 1].
	/// @param t Position along the gradient, clamped to range [0, 1].
	/// @return Interpolated color at position t or white if no stops are defined.
	[[nodiscard]] Color Sample(float t) const;

	/// @param t Position along the gradient, clamped to range [0, 1].
	void AddStop(float t, Color color);

private:
	std::vector<impl::ColorStop> stops_;
};

} // namespace ptgn