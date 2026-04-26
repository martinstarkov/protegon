#pragma once

#include <array>

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

class Capsule {
public:
	constexpr Capsule() = default;

	constexpr Capsule(V2_float start, V2_float end, float radius) :
		start_{ start }, end_{ end }, radius_{ radius } {}

	void SetStart(V2_float start);
	void SetEnd(V2_float end);
	void SetRadius(float radius);

	/// @param out_size Optional parameter for the unrotated size of the quad.
	/// @return Quad vertices relative to the given transform for this line with a given a line
	/// width.
	std::array<V2_float, 4> GetWorldQuadVertices(Transform transform, V2_float* out_size = nullptr)
		const;

	std::array<V2_float, 2> GetWorldVertices(Transform transform) const;

	std::array<V2_float, 2> GetLocalVertices() const;

	float GetRadius() const;

	/// @return Radius scaled relative to the transform.
	float GetRadius(Transform transform) const;

	V2_float GetStart() const;
	V2_float GetEnd() const;

	/// @brief Get direction from start to end.
	V2_float GetDirection() const;

	bool operator==(const Capsule&) const = default;

	PTGN_SERIALIZE(Capsule, start_, end_, radius_)

private:
	V2_float start_;
	V2_float end_;
	float radius_{ 0.0f };
};

} // namespace ptgn