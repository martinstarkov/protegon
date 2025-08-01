#pragma once

#include <array>

#include "components/drawable.h"
#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

class Entity;
struct Transform;

namespace impl {

class RenderData;

} // namespace impl

struct Capsule : public Drawable<Capsule> {
	Capsule() = default;

	Capsule(const V2_float& start, const V2_float& end, float radius);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	[[nodiscard]] std::array<V2_float, 2> GetWorldVertices(const Transform& transform) const;

	[[nodiscard]] std::array<V2_float, 2> GetLocalVertices() const;

	[[nodiscard]] float GetRadius() const;

	// @return Radius scaled relative to the transform.
	[[nodiscard]] float GetRadius(const Transform& transform) const;

	V2_float start;
	V2_float end;
	float radius{ 0.0f };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Capsule, start, end, radius)
};

} // namespace ptgn