#pragma once

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

struct Polygon : public Drawable<Polygon> {
	Polygon() = default;

	template <typename Container>
	explicit Polygon(const Container& points) {
		vertices.assign(points.begin(), points.end());
	}

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	[[nodiscard]] std::vector<V2_float> GetWorldVertices(const Transform& transform) const;

	[[nodiscard]] std::vector<V2_float> GetLocalVertices() const;

	// @return Centroid of the polygon.
	[[nodiscard]] V2_float GetCenter() const;

	std::vector<V2_float> vertices;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Polygon, vertices)
};

} // namespace ptgn