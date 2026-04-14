#pragma once

#include <array>

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "serialization/serialize.h"

namespace ptgn {

class Ellipse {
public:
	constexpr Ellipse() = default;

	template <Arithmetic T>
	constexpr explicit Ellipse(Vector2<T> ellipse_radius) : radius_{ ellipse_radius } {}

	void SetRadius(V2_float radius);

	/// @return Center relative to the world.
	V2_float GetCenter(Transform transform) const;

	V2_float GetRadius() const;

	/// @return Radius scaled relative to the transform.
	V2_float GetRadius(Transform transform) const;

	std::array<V2_float, 4> GetWorldQuadVertices(Transform transform) const;

	std::array<V2_float, 4> GetLocalQuadVertices() const;

	bool operator==(const Ellipse&) const = default;

	PTGN_REFLECT(Ellipse, radius_)

private:
	V2_float radius_;
};

} // namespace ptgn