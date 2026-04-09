#pragma once

#include <array>

#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/json/serialize.h"

namespace ptgn {

/// @brief RoundedRect has no rotation center because this can be achieved via using a parent Entity
/// and positioning it where the origin should be.
class RoundedRect {
public:
	constexpr RoundedRect() = default;

	constexpr RoundedRect(V2_float min, V2_float max, float radius) :
		min_{ min }, max_{ max }, radius_{ radius } {}

	constexpr RoundedRect(V2_float size, float radius) :
		RoundedRect{ -size * 0.5f, size * 0.5f, radius } {}

	void SetRadius(float radius);
	void SetSize(V2_float size);
	void SetSize(V2_float min, V2_float max);

	V2_float GetSize() const;
	float GetRadius() const;

	/// @return Size scaled relative to the transform.
	V2_float GetSize(Transform transform) const;
	float GetRadius(Transform transform) const;

	/// @return New transform offset by the draw_origin.
	[[nodiscard]] Transform Offset(Transform transform, Origin draw_origin) const;

	/// @return Quad vertices relative to the transform where transform.position is taken as the
	/// rounded rectangle center.
	std::array<V2_float, 4> GetWorldQuadVertices(Transform transform) const;
	std::array<V2_float, 4> GetLocalQuadVertices() const;

	std::array<V2_float, 4> GetWorldQuadVertices(Transform transform, Origin draw_origin) const;

	/// @return Center relative to the world.
	V2_float GetCenter(Transform transform) const;

	bool operator==(const RoundedRect&) const = default;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(RoundedRect, min_, max_, radius_)

private:
	V2_float min_;
	V2_float max_;
	float radius_{ 0.0f };
};

} // namespace ptgn