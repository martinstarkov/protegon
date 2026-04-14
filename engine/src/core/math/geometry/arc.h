#pragma once

#include <array>

#include "core/math/angle.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

class Arc {
public:
	constexpr Arc() = default;

	constexpr Arc(float arc_radius, Radians start_angle, Radians end_angle, bool clockwise = true) :
		radius_{ arc_radius },
		start_angle_{ start_angle },
		end_angle_{ end_angle },
		clockwise_{ clockwise } {}

	constexpr Arc(float arc_radius, Degrees start_angle, Degrees end_angle, bool clockwise = true) :
		Arc{ arc_radius, start_angle.ToRad(), end_angle.ToRad(), clockwise } {}

	void SetRadius(float radius);
	void SetStartAngle(Radians start_angle);
	void SetEndAngle(Radians end_angle);
	void SetStartAngle(Degrees start_angle);
	void SetEndAngle(Degrees end_angle);
	void SetClockwise(bool clockwise = true);

	/// @return Center relative to the world.
	V2_float GetCenter(Transform transform) const;

	float GetRadius() const;
	Degrees GetStartAngle() const;
	Degrees GetEndAngle() const;
	Degrees GetAperture() const;
	[[nodiscard]] bool IsClockwise() const;

	/// @return Radius scaled relative to the transform.
	float GetRadius(Transform transform) const;

	std::array<V2_float, 4> GetWorldQuadVertices(Transform transform) const;

	std::array<V2_float, 4> GetLocalQuadVertices() const;

	bool operator==(const Arc&) const = default;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Arc, radius_, start_angle_, end_angle_, clockwise_)

private:
	float radius_{ 0.0f };
	Radians start_angle_{ 0.0f };
	Radians end_angle_{ 0.0f };

	/// @brief Direction of arc.
	bool clockwise_{ true };
};

} // namespace ptgn