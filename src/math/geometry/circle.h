#pragma once

#include "collision/raycast.h"
#include "math/geometry/intersection.h"
#include "math/vector2.h"

namespace ptgn {

struct LayerInfo;
struct Color;
struct Rect;
struct Line;
struct Capsule;

struct Circle {
	V2_float center;
	float radius{ 0.0f };

	// Uses default render target.
	void Draw(const Color& color, float line_width = -1.0f) const;

	void Draw(const Color& color, float line_width, const LayerInfo& layer_info) const;

	// center += offset
	void Offset(const V2_float& offset);

	[[nodiscard]] V2_float Center() const;

	[[nodiscard]] bool Overlaps(const V2_float& point) const;
	[[nodiscard]] bool Overlaps(const Line& line) const;
	[[nodiscard]] bool Overlaps(const Circle& circle) const;
	[[nodiscard]] bool Overlaps(const Rect& rect) const;
	[[nodiscard]] bool Overlaps(const Capsule& capsule) const;

	[[nodiscard]] Intersection Intersects(const Circle& circle) const;
	[[nodiscard]] Intersection Intersects(const Rect& rect) const;

	[[nodiscard]] ptgn::Raycast Raycast(const V2_float& ray, const Line& line) const;
	[[nodiscard]] ptgn::Raycast Raycast(const V2_float& ray, const Circle& circle) const;
	[[nodiscard]] ptgn::Raycast Raycast(const V2_float& ray, const Capsule& capsule) const;
	[[nodiscard]] ptgn::Raycast Raycast(const V2_float& ray, const Rect& rect) const;

private:
};

struct Arc {
	V2_float center;
	float radius{ 0.0f };

	// Radians counter-clockwise from the right.
	float start_angle{ 0.0f };
	// Radians counter-clockwise from the right.
	float end_angle{ 0.0f };

	// Uses default render target.
	void Draw(
		bool clockwise, const Color& color, float line_width = -1.0f
	) const;

	void Draw(
		bool clockwise, const Color& color, float line_width,
		const LayerInfo& layer_info
	) const;
};

struct Ellipse {
	V2_float center;
	V2_float radius;

	// Uses default render target.
	void Draw(const Color& color, float line_width = -1.0f) const;

	void Draw(const Color& color, float line_width, const LayerInfo& layer_info) const;
};

} // namespace ptgn