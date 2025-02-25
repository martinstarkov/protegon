#pragma once

#include <array>

#include "core/game_object.h"
#include "ecs/ecs.h"
#include "math/collision/raycast.h"
#include "math/vector2.h"

namespace ptgn {

struct Color;
struct Circle;
struct Rect;
struct Capsule;
struct RoundedRect;

// Line start point is its transform position.
struct Line : public GameObject {
	Line() = default;
	Line(const ecs::Entity& e);
	Line(const ecs::Entity& e, const V2_float& start, const V2_float& end);

	// Set start point of line in world space. This will modify its transform position.
	Line& SetStart(const V2_float& start);

	// Set end point of the line in world space.
	Line& SetEnd(const V2_float& end);

	// Set the start and end points of the line in world space. This start position will be set as
	// the line's transform position.
	Line& SetVertices(const V2_float& start, const V2_float& end);

	[[nodiscard]] std::array<V2_float, 2> GetVertices() const;

	[[nodiscard]] V2_float GetStart() const;
	[[nodiscard]] V2_float GetEnd() const;

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

	[[nodiscard]] static std::array<V2_float, 4> GetQuadVertices(
		const V2_float& start, const V2_float& end, float line_width
	);

private:
	// End point relative to world space.
	V2_float end_;
};

// Capsule start point is its transform position.
struct Capsule : public GameObject {
	Capsule() = default;
	Capsule(const ecs::Entity& e);
	Capsule(const ecs::Entity& e, const V2_float& start, const V2_float& end, float radius);

	// Set start point of capsule in world space. This will modify its transform position.
	Capsule& SetStart(const V2_float& start);

	// Set end point of the capsule in world space.
	Capsule& SetEnd(const V2_float& end);

	Capsule& SetRadius(float radius);

	// Set the start and end points of the capsule in world space. This start position will be set
	// as the capsule's transform position.
	Capsule& SetVertices(const V2_float& start, const V2_float& end);

	[[nodiscard]] std::array<V2_float, 2> GetVertices() const;

	[[nodiscard]] V2_float GetStart() const;
	[[nodiscard]] V2_float GetEnd() const;
	[[nodiscard]] float GetRadius() const;

	[[nodiscard]] bool Overlaps(const V2_float& point) const;
	[[nodiscard]] bool Overlaps(const Line& line) const;
	[[nodiscard]] bool Overlaps(const Circle& circle) const;
	[[nodiscard]] bool Overlaps(const Capsule& capsule) const;
	[[nodiscard]] bool Overlaps(const Rect& rect) const;

private:
	V2_float end_;
	float radius_{ 0.0f };
};

} // namespace ptgn