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

struct Triangle {
	Triangle() = default;

	Triangle(const V2_float& a, const V2_float& b, const V2_float& c);
	explicit Triangle(const std::array<V2_float, 3>& vertices);

	static void Draw(impl::RenderData& ctx, const Entity& entity);

	[[nodiscard]] std::array<V2_float, 3> GetWorldVertices(const Transform& transform) const;

	[[nodiscard]] std::array<V2_float, 3> GetLocalVertices() const;

	V2_float a;
	V2_float b;
	V2_float c;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Triangle, a, b, c)
};

PTGN_DRAWABLE_REGISTER(Triangle);

} // namespace ptgn