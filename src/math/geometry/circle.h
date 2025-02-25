#pragma once

#include <array>
#include <vector>

#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/collision/intersect.h"
#include "math/collision/raycast.h"
#include "math/vector2.h"

namespace ptgn {

struct Color;
struct Rect;
struct Line;
struct Capsule;
struct RoundedRect;

struct Circle : public GameObject {
	Circle() = default;
	Circle(const ecs::Entity& e);
	Circle(const ecs::Entity& e, float radius);

	Circle& SetRadius(float radius);
	[[nodiscard]] float GetRadius() const;

	[[nodiscard]] V2_float GetCenter() const;

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
	float radius_{ 0.0f };
};

struct Arc : public GameObject {
	Arc() = default;
	Arc(const ecs::Entity& e);

	float radius{ 0.0f };

	// Radians counter-clockwise from the right.
	float start_angle{ 0.0f };
	// Radians counter-clockwise from the right.
	float end_angle{ 0.0f };

private:
	friend struct RoundedRect;
	friend struct Capsule;

	// @param clockwise Whether the vertices are in clockwise direction (true), or counter-clockwise
	// (false).
	// @param sa start_angle clamped between 0 and 2 pi.
	// @param ea end_angle clamped between 0 and 2 pi.
	// @return The vertices which make up the arc.
	[[nodiscard]] std::vector<V2_float> GetVertices(bool clockwise, float sa, float ea) const;
};

struct Ellipse : public GameObject {
	Ellipse() = default;
	Ellipse(const ecs::Entity& e);
	Ellipse(const ecs::Entity& e, const V2_float& radius);

	Ellipse& SetRadius(const V2_float& radius);
	[[nodiscard]] V2_float GetRadius() const;

	[[nodiscard]] V2_float GetCenter() const;

private:
	V2_float radius_;
};

} // namespace ptgn