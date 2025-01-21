#pragma once

#include <array>
#include <cstdint>

#include "math/raycast.h"
#include "math/vector2.h"
#include "math/vector4.h"

namespace ptgn {

struct Color;
struct Circle;
struct Rect;
struct Capsule;
struct RoundedRect;

struct Line {
	V2_float a;
	V2_float b;

	[[nodiscard]] bool operator==(const Line& o) const {
		return a == o.a && b == o.b;
	}

	[[nodiscard]] bool operator!=(const Line& o) const {
		return !(*this == o);
	}

	void Draw(const Color& color, float line_width = 1.0f, std::int32_t render_layer = 0) const;

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

	// @param line_width The width of the line to create a quad for.
	// @return Return the vertices required to draw a solid rotated quad to mimic a line with the
	// given width.
	[[nodiscard]] std::array<V2_float, 4> GetQuadVertices(float line_width) const;

private:
	friend struct Capsule;
	friend struct RoundedRect;

	void DrawSolid(const V4_float& color, std::int32_t render_layer) const;

	void DrawThick(float line_width, const V4_float& color, std::int32_t render_layer) const;
};

struct Capsule {
	Line line;
	float radius{ 0.0f };

	Capsule() = default;

	Capsule(const Line& line, float radius) : line{ line }, radius{ radius } {}

	Capsule(const V2_float& a, const V2_float& b, float radius) : line{ a, b }, radius{ radius } {}

	// @param line_width -1 for a solid filled capsule.
	void Draw(const Color& color, float line_width = -1.0f, std::int32_t render_layer = 0) const;

	[[nodiscard]] bool Overlaps(const V2_float& point) const;
	[[nodiscard]] bool Overlaps(const Line& line) const;
	[[nodiscard]] bool Overlaps(const Circle& circle) const;
	[[nodiscard]] bool Overlaps(const Capsule& capsule) const;
	[[nodiscard]] bool Overlaps(const Rect& rect) const;
};

} // namespace ptgn
