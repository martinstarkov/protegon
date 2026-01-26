#pragma once

#include <array>

#include "core/util/concepts.h"
#include "ecs/components/origin.h"
#include "math/vector2.h"
#include "serialization/json/serialize.h"

namespace ptgn {

struct Transform;

// Rect has no rotation center because this can be achieved via using a parent Entity and
// positioning it where the origin should be.
struct Rect {
	Rect() = default;

	Rect(V2_float min, V2_float max);

	template <Arithmetic T>
	Rect(const Vector2<T>& size) : min{ -size * 0.5f }, max{ size * 0.5f } {}

	template <Arithmetic T>
	Rect(T x, T y) : Rect{ Vector2<T>{ x, y } } {}

	[[nodiscard]] V2_float GetSize() const;

	// @return Size scaled relative to the transform.
	[[nodiscard]] V2_float GetSize(const Transform& transform) const;

	// @return New transform offset by the draw_origin.
	[[nodiscard]] Transform Offset(const Transform& transform, Origin draw_origin) const;

	// @return Quad vertices relative to the transform where transform.position is taken as the
	// rectangle center.
	[[nodiscard]] std::array<V2_float, 4> GetWorldVertices(const Transform& transform) const;
	[[nodiscard]] std::array<V2_float, 4> GetLocalVertices() const;

	[[nodiscard]] std::array<V2_float, 4> GetWorldVertices(
		const Transform& transform, Origin draw_origin
	) const;

	// @return Center relative to the world.
	[[nodiscard]] V2_float GetCenter(const Transform& transform) const;

	bool operator==(const Rect&) const = default;

	V2_float min;
	V2_float max;

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Rect, min, max)
};

} // namespace ptgn