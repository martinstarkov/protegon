#pragma once

#include "collision/raycast.h"
#include "math/vector2.h"
#include "renderer/layer_info.h"

namespace ptgn {

struct Color;
struct Circle;
struct Rect;
struct Capsule;

struct Line {
	V2_float a;
	V2_float b;

	[[nodiscard]] bool operator==(const Line& o) const {
		return a == o.a && b == o.b;
	}

	[[nodiscard]] bool operator!=(const Line& o) const {
		return !(*this == o);
	}

	void Draw(const Color& color, float line_width = 1.0f, const LayerInfo& layer_info = {}) const;

	[[nodiscard]] V2_float Direction() const;
	[[nodiscard]] V2_float Midpoint() const;

	[[nodiscard]] bool Contains(const Line& line) const;

	[[nodiscard]] bool Overlaps(const V2_float& point) const;
	[[nodiscard]] bool Overlaps(const Line& line) const;
	[[nodiscard]] bool Overlaps(const Circle& circle) const;
	[[nodiscard]] bool Overlaps(const Rect& rect) const;
	[[nodiscard]] bool Overlaps(const Capsule& capsule) const;

	[[nodiscard]] ptgn::Raycast Raycast(const Line& line) const;
	[[nodiscard]] ptgn::Raycast Raycast(const Circle& circle) const;
	[[nodiscard]] ptgn::Raycast Raycast(const Rect& rect) const;
	[[nodiscard]] ptgn::Raycast Raycast(const Capsule& capsule) const;
};

struct Capsule {
	Capsule() = default;

	Capsule(const Line& line, float radius) : line{ line }, radius{ radius } {}

	Capsule(const V2_float& a, const V2_float& b, float radius) : line{ a, b }, radius{ radius } {}

	Line line;
	float radius{ 0.0f };

	void Draw(const Color& color, float line_width = 1.0f, const LayerInfo& layer_info = {}) const;

	[[nodiscard]] bool Overlaps(const V2_float& point) const;
	[[nodiscard]] bool Overlaps(const Line& line) const;
	[[nodiscard]] bool Overlaps(const Circle& circle) const;
	[[nodiscard]] bool Overlaps(const Capsule& capsule) const;
	[[nodiscard]] bool Overlaps(const Rect& rect) const;
};

} // namespace ptgn
