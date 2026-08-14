#pragma once

#include <array>

#include "core/assert.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "serialization/serialize.h"

namespace ptgn {

class Line {
public:
	V2_float start;
	V2_float end;

	constexpr Line() = default;

	constexpr Line(V2_float start, V2_float end) : start{ start }, end{ end } {}

	/// @param out_size Optional parameter for the unrotated size of the quad.
	/// @return Quad vertices relative to the given transform for this line with a given a line
	/// width.
	constexpr std::array<V2_float, 4> GetWorldQuadVertices(
		Transform transform, float line_width = 1.0f, V2_float* out_size = nullptr
	) const {
		PTGN_ASSERT(line_width >= 1.0f);

		const auto dir{ GetDirection() };
		const float length{ dir.Magnitude() };

		Rect rect{ V2_float{ length + line_width, line_width } };

		const Transform line_transform{
			start + dir * 0.5f,
			dir.Angle(),
			V2_float{ 1.0f },
		};

		auto vertices{ rect.GetWorldVertices(line_transform) };

		if (out_size) {
			*out_size = rect.GetSize(transform);
		}

		return transform.Apply(vertices);
	}

	constexpr std::array<V2_float, 2> GetWorldVertices(Transform transform) const {
		auto local_vertices{ GetLocalVertices() };
		return transform.Apply(local_vertices);
	}

	constexpr std::array<V2_float, 2> GetLocalVertices() const {
		return { start, end };
	}

	/// @brief Get direction from start to end.
	constexpr V2_float GetDirection() const {
		return end - start;
	}

	constexpr bool operator==(const Line&) const = default;

	PTGN_REFLECT(Line, start, end)
};

} // namespace ptgn