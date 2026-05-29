#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <ranges>

#include "core/assert.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "serialization/serialize.h"

namespace ptgn {

/// @brief Rect has no rotation center because this can be achieved via using a parent Entity and
/// positioning it where the origin should be.
class Rect {
public:
	V2_float min;
	V2_float max;

	constexpr Rect() = default;

	constexpr Rect(V2_float min, V2_float max) : min{ min }, max{ max } {}

	template <Arithmetic T>
	constexpr Rect(Vector2<T> size) : min{ -size * 0.5f }, max{ size * 0.5f } {} // NOSONAR

	template <Arithmetic T>
	constexpr Rect(T x, T y) : Rect{ Vector2<T>{ x, y } } {}

	constexpr bool HasPositiveArea() const {
		return max.x > min.x && max.y > min.y;
	}

	/// @return New Rect with min and max expanded by the +margin.
	[[nodiscard]] constexpr Rect Expanded(V2_float margin) const {
		return {
			min - margin,
			max + margin,
		};
	}

	/// @return New Rect with min and max translated by the +offset.
	[[nodiscard]] constexpr Rect Translated(V2_float offset) const {
		return {
			min + offset,
			max + offset,
		};
	}

	constexpr V2_float GetSize() const {
		return max - min;
	}

	/// @return Size scaled relative to the transform.
	constexpr V2_float GetSize(Transform transform) const {
		auto size{ GetSize() };
		auto abs_scale{ Abs(transform.scale) };
		return size * abs_scale;
	}

	/// @return Center relative to the local shape.
	constexpr V2_float GetCenter() const {
		return (max + min) * 0.5f;
	}

	/// @return Center relative to the transform.
	constexpr V2_float GetCenter(Transform transform) const {
		auto center{ GetCenter() };
		return center + transform.position;
	}

	/// @return New transform offset by the draw_origin.
	[[nodiscard]] constexpr Transform Offset(Transform transform, Origin draw_origin) const {
		auto size{ GetSize(transform) };
		auto offset{ GetOffset(draw_origin, size) };

		if (offset.IsZero()) {
			return transform;
		}

		transform.Translate(offset);

		return transform;
	}

	/// @return Quad vertices relative to the transform where transform.position is taken as the
	/// rectangle center.
	constexpr std::array<V2_float, 4> GetWorldVertices(Transform transform) const {
		auto local_vertices{ GetLocalVertices() };
		return transform.Apply(local_vertices);
	}

	constexpr std::array<V2_float, 4> GetLocalVertices() const {
		PTGN_ASSERT(min != max, "Cannot get local vertices for a rect with size zero");
		return { min, V2_float{ max.x, min.y }, max, V2_float{ min.x, max.y } };
	}

	constexpr std::array<V2_float, 4> GetWorldVertices(
		Transform transform, Origin draw_origin
	) const {
		auto offset_transform{ Offset(transform, draw_origin) };
		return GetWorldVertices(offset_transform);
	}

	/// @return A Rect that bounds the given points, where the min and max are axis-aligned with the
	/// world axes.
	template <std::ranges::input_range TRange>
		requires std::convertible_to<std::ranges::range_reference_t<TRange>, V2_float>
	[[nodiscard]] constexpr static Rect FromPoints(TRange&& points) {
		auto it{ std::ranges::begin(points) };
		auto end{ std::ranges::end(points) };

		PTGN_ASSERT(it != end, "Must provide at least one point to form bounds from");

		Rect bounds{
			*it,
			*it,
		};

		// Skip the first element since it is the default value.
		++it;

		PTGN_ASSERT(it != end, "Must provide at least two points to form bounds from");

		auto expand_to_include = [&](V2_float point) {
			bounds.min = Min(bounds.min, point);
			bounds.max = Max(bounds.max, point);
		};

		std::ranges::for_each(std::ranges::subrange{ it, end }, expand_to_include);

		return bounds;
	}

	constexpr bool operator==(const Rect&) const = default;

	PTGN_SERIALIZE(Rect, min, max)
};

} // namespace ptgn