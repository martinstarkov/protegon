#pragma once

#include <array>

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

class Line {
public:
	constexpr Line() = default;

	constexpr Line(V2_float start, V2_float end) : start_{ start }, end_{ end } {}

	void SetStart(V2_float start);
	void SetEnd(V2_float end);

	/// @param out_size Optional parameter for the unrotated size of the quad.
	/// @return Quad vertices relative to the given transform for this line with a given a line
	/// width.
	std::array<V2_float, 4> GetWorldQuadVertices(
		Transform transform, float line_width = 1.0f, V2_float* out_size = nullptr
	) const;

	std::array<V2_float, 2> GetWorldVertices(Transform transform) const;

	std::array<V2_float, 2> GetLocalVertices() const;

	V2_float GetStart() const;
	V2_float GetEnd() const;

	/// @brief Get direction from start to end.
	V2_float GetDirection() const;

	bool operator==(const Line&) const = default;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Line, start_, end_)

private:
	V2_float start_;
	V2_float end_;
};

} // namespace ptgn