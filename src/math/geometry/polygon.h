#pragma once

#include <concepts>
#include <ranges>
#include <vector>

#include "components/drawable.h"
#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

class Entity;
struct Transform;

namespace impl {

class RenderData;

} // namespace impl

struct Polygon {
	Polygon() = default;

	template <typename Container>
		requires std::ranges::input_range<Container> &&
				 std::convertible_to<std::ranges::range_value_t<Container>, V2_float>
	Polygon(const Container& points) {
		vertices.assign(points.begin(), points.end());
	}

	static void Draw(const Entity& entity);

	[[nodiscard]] std::vector<V2_float> GetWorldVertices(const Transform& transform) const;

	[[nodiscard]] std::vector<V2_float> GetLocalVertices() const;

	// @return Centroid of the polygon.
	[[nodiscard]] V2_float GetCenter() const;

	bool operator==(const Polygon&) const = default;

	std::vector<V2_float> vertices;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Polygon, vertices)
};

PTGN_DRAWABLE_REGISTER(Polygon);

} // namespace ptgn