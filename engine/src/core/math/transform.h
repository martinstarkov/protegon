#pragma once

#include <array>
#include <functional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/math/angle.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "serialization/serialize.h"

namespace ptgn {

constexpr float kMinScale{ 0.001f };
constexpr float kMaxScale{ 10000.0f };

struct Transform {
	V2_float position;

	/// @brief Positive clockwise.
	Radians rotation{ 0.0f };

	/// @brief Can be negative but not zero. Negative scale will flip the transform across the
	/// corresponding axis.
	V2_float scale{ 1.0f, 1.0f };

	constexpr Transform() = default;

	template <Arithmetic T>
	constexpr Transform(Vector2<T> position) : position{ position } {} // NOSONAR

	constexpr Transform(V2_float position, Radians rotation, V2_float scale = { 1.0f, 1.0f }) :
		position{ position }, rotation{ rotation }, scale{ scale } {}

	constexpr Transform(V2_float position, Degrees rotation, V2_float scale = { 1.0f, 1.0f }) :
		Transform{ position, rotation.ToRad(), scale } {}

	constexpr bool IsIdentity() const {
		return *this == Transform{};
	}

	[[nodiscard]] constexpr Transform Inverse() const {
		PTGN_ASSERT(!scale.HasZero(), "Cannot get inverse of transform with zero");
		return { -position, -rotation, 1.0f / scale };
	}

	constexpr Transform& Translate(V2_float delta) {
		position += delta;
		return *this;
	}

	constexpr Transform& Scale(V2_float delta) {
		scale *= delta;
		ClampScale();
		return *this;
	}

	constexpr Transform& Rotate(Radians delta) {
		rotation += delta;
		return *this;
	}

	constexpr Transform& Rotate(Degrees delta) {
		return Rotate(delta.ToRad());
	}

	[[nodiscard]] constexpr Transform RelativeTo(Transform parent) const {
		Transform result;
		// Order is important.
		result.scale	= parent.scale * scale;
		result.rotation = parent.rotation + rotation;
		result.position = parent.position + (parent.scale * position).Rotated(parent.rotation);
		return result;
	}

	[[nodiscard]] constexpr Transform InverseRelativeTo(Transform parent) const {
		Transform local;

		auto inv_rotation{ -parent.rotation };
		V2_float inv_scale{ parent.scale.x != 0 ? 1.0f / parent.scale.x : 0.0f,
							parent.scale.y != 0 ? 1.0f / parent.scale.y : 0.0f };

		auto delta{ position - parent.position };

		// Unrotate and unscale the position.
		local.position	= delta.Rotated(inv_rotation);
		local.position *= inv_scale;

		local.rotation = rotation - parent.rotation;
		local.scale	   = scale * inv_scale;

		return local;
	}

	constexpr bool operator==(const Transform&) const = default;

	constexpr bool HasRotation() const {
		return rotation.value != 0.0f;
	}

	/// @brief Clamps rotation between [0, 360 deg).
	constexpr Transform& ClampRotation() {
		rotation = Clamp(rotation);
		return *this;
	}

	/// @brief Clamps scale between [kMinScale, kMaxScale].
	constexpr Transform& ClampScale() {
		scale = Clamp(scale, kMinScale, kMaxScale);
		return *this;
	}

	/// @return abs(scale_x + scale_y) / 2
	constexpr float GetAverageScale() const {
		// Absolute value applied because negative scale is used for flip.
		auto abs_scale{ Abs(scale) };
		return (abs_scale.x + abs_scale.y) * 0.5f;
	}

	constexpr void ApplyTo(V2_float& point) const {
		if (HasRotation()) {
			point = ApplyWithRotation(point, rotation.Cos(), rotation.Sin());
			return;
		}
		if (!IsIdentity()) {
			point = ApplyWithoutRotation(point);
		}
	}

	constexpr void ApplyTo(std::span<V2_float> points) const {
		WithPointTransform<Direction::Forward>([&points]<typename T>(T&& transform) {
			std::ranges::transform(points, points.begin(), std::forward<T>(transform));
		});
	}

	template <
		std::ranges::input_range TRange,
		InvocableR<V2_float, std::ranges::range_const_reference_t<TRange>> TGetPosition,
		InvocableR<void, std::ranges::range_reference_t<TRange>, V2_float> TSetPosition>
	constexpr void ApplyTo(
		TRange&& elements, TGetPosition&& get_position, TSetPosition&& set_position
	) const {
		ApplyToElements<Direction::Forward>(
			std::forward<TRange>(elements), std::forward<TGetPosition>(get_position),
			std::forward<TSetPosition>(set_position)
		);
	}

	constexpr void ApplyInverseTo(V2_float& point) const {
		if (HasRotation()) {
			point = ApplyInverseWithRotation(point, rotation.Cos(), rotation.Sin());
			return;
		}
		if (!IsIdentity()) {
			point = ApplyInverseWithoutRotation(point);
		}
	}

	constexpr void ApplyInverseTo(std::span<V2_float> points) const {
		WithPointTransform<Direction::Inverse>([&points]<typename T>(T&& transform) {
			std::ranges::transform(points, points.begin(), std::forward<T>(transform));
		});
	}

	template <
		std::ranges::input_range TRange,
		InvocableR<V2_float, std::ranges::range_reference_t<TRange>> TGetPosition,
		InvocableR<void, std::ranges::range_reference_t<TRange>, V2_float> TSetPosition>
	constexpr void ApplyInverseTo(
		TRange&& elements, TGetPosition&& get_position, TSetPosition&& set_position
	) const {
		ApplyToElements<Direction::Inverse>(
			std::forward<TRange>(elements), std::forward<TGetPosition>(get_position),
			std::forward<TSetPosition>(set_position)
		);
	}

	[[nodiscard]] constexpr V2_float Apply(V2_float point) const {
		WithPointTransform<Direction::Forward>([&point](auto&& transform) {
			point = transform(point);
		});
		return point;
	}

	[[nodiscard]] constexpr std::vector<V2_float> Apply(std::span<const V2_float> points) const {
		std::vector<V2_float> transformed_points(points.size());
		Apply(points, transformed_points);
		return transformed_points;
	}

	template <std::size_t N>
	[[nodiscard]] constexpr std::array<V2_float, N> Apply(
		const std::array<V2_float, N>& points
	) const {
		std::array<V2_float, N> transformed_points;
		Apply(points, transformed_points);
		return transformed_points;
	}

	template <
		typename TContainer,
		InvocableR<V2_float, std::ranges::range_reference_t<TContainer&>> TGetPosition,
		InvocableR<void, std::ranges::range_reference_t<TContainer&>, V2_float> TSetPosition>
	[[nodiscard]] constexpr TContainer Apply(
		TContainer elements, TGetPosition&& get_position, TSetPosition&& set_position
	) const {
		ApplyTo(
			elements, std::forward<TGetPosition>(get_position),
			std::forward<TSetPosition>(set_position)
		);
		return elements;
	}

	[[nodiscard]] constexpr V2_float ApplyInverse(V2_float point) const {
		WithPointTransform<Direction::Inverse>([&point](auto&& transform) {
			point = transform(point);
		});
		return point;
	}

	[[nodiscard]] constexpr std::vector<V2_float> ApplyInverse(
		std::span<const V2_float> points
	) const {
		std::vector<V2_float> transformed_points(points.size());
		ApplyInverse(points, transformed_points);
		return transformed_points;
	}

	template <std::size_t N>
	[[nodiscard]] constexpr std::array<V2_float, N> ApplyInverse(
		const std::array<V2_float, N>& points
	) const {
		std::array<V2_float, N> transformed_points;
		ApplyInverse(points, transformed_points);
		return transformed_points;
	}

	template <
		typename TContainer,
		InvocableR<V2_float, std::ranges::range_reference_t<TContainer&>> TGetPosition,
		InvocableR<void, std::ranges::range_reference_t<TContainer&>, V2_float> TSetPosition>
	[[nodiscard]] constexpr TContainer ApplyInverse(
		TContainer elements, TGetPosition&& get_position, TSetPosition&& set_position
	) const {
		ApplyInverseTo(
			elements, std::forward<TGetPosition>(get_position),
			std::forward<TSetPosition>(set_position)
		);
		return elements;
	}

	enum class Direction {
		Forward,
		Inverse
	};

	template <Direction Dir, typename F>
	constexpr void WithPointTransform(F&& function) const {
		if (IsIdentity()) {
			std::invoke(std::forward<F>(function), [](V2_float point) { return point; });
			return;
		}

		if (!HasRotation()) {
			std::invoke(std::forward<F>(function), [this](V2_float point) {
				return ApplyWithoutRotationImpl<Dir>(point);
			});
		} else {
			float cos{ rotation.Cos() };
			float sin{ rotation.Sin() };

			std::invoke(std::forward<F>(function), [this, cos, sin](V2_float point) {
				return ApplyWithRotationImpl<Dir>(point, cos, sin);
			});
		}
	}

	PTGN_SERIALIZE(Transform, position, rotation, scale)
private:
	constexpr void Apply(
		std::span<const V2_float> points, std::span<V2_float> out_transformed_points
	) const {
		PTGN_ASSERT(out_transformed_points.size() >= points.size());

		WithPointTransform<Direction::Forward>(
			[&points, &out_transformed_points]<typename T>(T&& transform) {
				std::ranges::transform(
					points, out_transformed_points.begin(), std::forward<T>(transform)
				);
			}
		);
	}

	constexpr void ApplyInverse(
		std::span<const V2_float> points, std::span<V2_float> out_transformed_points
	) const {
		PTGN_ASSERT(out_transformed_points.size() >= points.size());

		WithPointTransform<Direction::Inverse>(
			[&points, &out_transformed_points]<typename T>(T&& transform) {
				std::ranges::transform(
					points, out_transformed_points.begin(), std::forward<T>(transform)
				);
			}
		);
	}

	[[nodiscard]] constexpr V2_float ApplyWithRotation(V2_float point, float cos, float sin) const {
		PTGN_ASSERT(!scale.HasZero(), "Cannot transform point for an object with zero ");
		return position + (scale * point).Rotated(cos, sin);
	}

	[[nodiscard]] constexpr V2_float ApplyWithoutRotation(V2_float point) const {
		PTGN_ASSERT(!scale.HasZero(), "Cannot transform point for an object with zero");
		return position + scale * point;
	}

	[[nodiscard]] constexpr V2_float ApplyInverseWithRotation(
		V2_float point, float cos, float sin
	) const {
		PTGN_ASSERT(!scale.HasZero(), "Cannot inverse transform point for an object with zero");

		return (point - position).Rotated(cos, -sin) / scale;
	}

	[[nodiscard]] constexpr V2_float ApplyInverseWithoutRotation(V2_float point) const {
		PTGN_ASSERT(!scale.HasZero(), "Cannot inverse transform point for an object with zero");

		return (point - position) / scale;
	}

	template <Direction Dir>
	[[nodiscard]] constexpr V2_float ApplyWithRotationImpl(
		V2_float point, float cos, float sin
	) const {
		if constexpr (Dir == Direction::Forward) {
			return ApplyWithRotation(point, cos, sin);
		} else {
			return ApplyInverseWithRotation(point, cos, sin);
		}
	}

	template <Direction Dir>
	[[nodiscard]] constexpr V2_float ApplyWithoutRotationImpl(V2_float point) const {
		if constexpr (Dir == Direction::Forward) {
			return ApplyWithoutRotation(point);
		} else {
			return ApplyInverseWithoutRotation(point);
		}
	}

	template <
		Direction Dir, std::ranges::input_range TRange,
		InvocableR<V2_float, std::ranges::range_reference_t<TRange>> TGetPosition,
		InvocableR<void, std::ranges::range_reference_t<TRange>, V2_float> TSetPosition>
	constexpr void ApplyToElements(
		TRange&& elements, TGetPosition get_position, TSetPosition set_position
	) const {
		WithPointTransform<Dir>([&elements, &get_position, &set_position](auto&& transform) {
			for (auto&& element : elements) {
				auto position{ std::invoke(get_position, element) };
				std::invoke(set_position, element, transform(position));
			}
		});
	}
};

} // namespace ptgn