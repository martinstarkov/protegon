#pragma once

#include <array>

#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "serialization/json/serialize.h"

namespace ptgn {

/// @brief Rect has no rotation center because this can be achieved via using a parent Entity and
/// positioning it where the origin should be.
class Rect {
public:
	Rect() = default;

	Rect(V2_float min, V2_float max);

	template <Arithmetic T>
	Rect(Vector2<T> size) : min_{ -size * 0.5f }, max_{ size * 0.5f } {} // NOSONAR

	template <Arithmetic T>
	Rect(T x, T y) : Rect{ Vector2<T>{ x, y } } {}

	void SetSize(V2_float size);
	void SetSize(V2_float min, V2_float max);

	V2_float GetSize() const;

	/// @return Size scaled relative to the transform.
	V2_float GetSize(Transform transform) const;

	/// @return New transform offset by the draw_origin.
	[[nodiscard]] Transform Offset(Transform transform, Origin draw_origin) const;

	/// @return Quad vertices relative to the transform where transform.position is taken as the
	/// rectangle center.
	std::array<V2_float, 4> GetWorldVertices(Transform transform) const;
	std::array<V2_float, 4> GetLocalVertices() const;

	std::array<V2_float, 4> GetWorldVertices(Transform transform, Origin draw_origin) const;

	/// @return Center relative to the world.
	V2_float GetCenter(Transform transform) const;

	bool operator==(const Rect&) const = default;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Rect, min_, max_)

private:
	V2_float min_;
	V2_float max_;
};

} // namespace ptgn