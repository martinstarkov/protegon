#pragma once

#include "math/vector2.h"

namespace ptgn {

struct Circle;
struct Rect;

struct Line {
	V2_float a;
	V2_float b;

	[[nodiscard]] V2_float Direction() const;
	[[nodiscard]] V2_float Midpoint() const;

	[[nodiscard]] bool Overlaps(const V2_float& point) const;
	[[nodiscard]] bool Overlaps(const Line& line) const;
	[[nodiscard]] bool Overlaps(const Circle& circle) const;
	[[nodiscard]] bool Overlaps(const Rect& rect) const;
};

struct Capsule {
	Line line;
	float radius{ 0.0f };
};

} // namespace ptgn
