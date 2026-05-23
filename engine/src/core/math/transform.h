#pragma once

#include <array>
#include <functional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "core/math/angle.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "serialization/serialize.h"

namespace ptgn {

constexpr float kMinScale{ 0.001f };
constexpr float kMaxScale{ 10000.0f };

struct Transform {
	Transform() = default;

	template <Arithmetic T>
	Transform(Vector2<T> position) : position_{ position } {} // NOSONAR

	Transform(V2_float position, Radians rotation, V2_float scale = { 1.0f, 1.0f });
	Transform(V2_float position, Degrees rotation, V2_float scale = { 1.0f, 1.0f });

	[[nodiscard]] bool IsIdentity() const;

	[[nodiscard]] Transform Inverse() const;

	[[nodiscard]] Transform RelativeTo(Transform parent) const;

	[[nodiscard]] Transform InverseRelativeTo(Transform parent) const;

	bool operator==(const Transform&) const = default;

	V2_float GetPosition() const;

	Transform& SetPosition(V2_float position);
	/// @brief Set position along a particular axis: x == 0, y == 1.
	Transform& SetPosition(std::size_t index, float position);
	Transform& SetPositionX(float x);
	Transform& SetPositionY(float y);

	/// @brief position += position_difference
	Transform& Translate(V2_float position_difference);
	Transform& TranslateX(float position_x_difference);
	Transform& TranslateY(float position_y_difference);

	/// @return Direction: Clockwise positive.
	Degrees GetRotation() const;

	[[nodiscard]] bool HasRotation() const;

	/// @param rotation Direction: Clockwise positive.
	Transform& SetRotation(Radians rotation);
	Transform& SetRotation(Degrees rotation);

	/// @brief rotation += angle_difference
	/// @param angle_difference Direction: Clockwise positive.
	Transform& Rotate(Radians angle_difference);
	Transform& Rotate(Degrees angle_difference);

	/// @brief Clamps rotation between [0, 360 deg).
	Transform& ClampRotation();

	/// @return abs(scale_x + scale_y) / 2
	float GetAverageScale() const;

	V2_float GetScale() const;

	Transform& SetScale(float scale);
	Transform& SetScale(V2_float scale);
	Transform& SetScaleX(float x);
	Transform& SetScaleY(float y);

	/// @brief scale *= scale_multiplier
	Transform& Scale(V2_float scale_multiplier);
	Transform& ScaleX(float scale_x_multiplier);
	Transform& ScaleY(float scale_y_multiplier);

	void ApplyTo(V2_float& point) const;
	void ApplyTo(std::span<V2_float> points) const;

	template <
		std::ranges::input_range TRange,
		InvocableR<V2_float, std::ranges::range_const_reference_t<TRange>> TGetPosition,
		InvocableR<void, std::ranges::range_reference_t<TRange>, V2_float> TSetPosition>
	void ApplyTo(
		TRange&& elements, TGetPosition&& get_position, TSetPosition&& set_position
	) const {
		ApplyToElements<TransformDirection::Forward>(
			std::forward<TRange>(elements), std::forward<TGetPosition>(get_position),
			std::forward<TSetPosition>(set_position)
		);
	}

	void ApplyInverseTo(V2_float& point) const;
	void ApplyInverseTo(std::span<V2_float> points) const;

	template <
		std::ranges::input_range TRange,
		InvocableR<V2_float, std::ranges::range_reference_t<TRange>> TGetPosition,
		InvocableR<void, std::ranges::range_reference_t<TRange>, V2_float> TSetPosition>
	void ApplyInverseTo(
		TRange&& elements, TGetPosition&& get_position, TSetPosition&& set_position
	) const {
		ApplyToElements<TransformDirection::Inverse>(
			std::forward<TRange>(elements), std::forward<TGetPosition>(get_position),
			std::forward<TSetPosition>(set_position)
		);
	}

	[[nodiscard]] V2_float Apply(V2_float point) const;

	[[nodiscard]] std::vector<V2_float> Apply(std::span<const V2_float> points) const;

	template <std::size_t N>
	[[nodiscard]] std::array<V2_float, N> Apply(const std::array<V2_float, N>& points) const {
		std::array<V2_float, N> transformed_points;
		Apply(points, transformed_points);
		return transformed_points;
	}

	template <
		typename TContainer,
		InvocableR<V2_float, std::ranges::range_reference_t<TContainer&>> TGetPosition,
		InvocableR<void, std::ranges::range_reference_t<TContainer&>, V2_float> TSetPosition>
	[[nodiscard]] TContainer Apply(
		TContainer elements, TGetPosition&& get_position, TSetPosition&& set_position
	) const {
		ApplyTo(
			elements, std::forward<TGetPosition>(get_position),
			std::forward<TSetPosition>(set_position)
		);
		return elements;
	}

	[[nodiscard]] V2_float ApplyInverse(V2_float point) const;

	[[nodiscard]] std::vector<V2_float> ApplyInverse(std::span<const V2_float> points) const;

	template <std::size_t N>
	[[nodiscard]] std::array<V2_float, N> ApplyInverse(
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
	[[nodiscard]] TContainer ApplyInverse(
		TContainer elements, TGetPosition&& get_position, TSetPosition&& set_position
	) const {
		ApplyInverseTo(
			elements, std::forward<TGetPosition>(get_position),
			std::forward<TSetPosition>(set_position)
		);
		return elements;
	}

	PTGN_SERIALIZE(Transform, position_, rotation_, scale_)
private:
	void Apply(std::span<const V2_float> points, std::span<V2_float> out_transformed_points) const;

	void ApplyInverse(
		std::span<const V2_float> points, std::span<V2_float> out_transformed_points
	) const;

	enum class TransformDirection {
		Forward,
		Inverse
	};

	[[nodiscard]] V2_float ApplyWithRotation(V2_float point, float cos, float sin) const;

	[[nodiscard]] V2_float ApplyWithoutRotation(V2_float point) const;

	[[nodiscard]] V2_float ApplyInverseWithRotation(V2_float point, float cos, float sin) const;

	[[nodiscard]] V2_float ApplyInverseWithoutRotation(V2_float point) const;

	template <TransformDirection Direction>
	[[nodiscard]] V2_float ApplyWithRotationImpl(V2_float point, float cos, float sin) const {
		if constexpr (Direction == TransformDirection::Forward) {
			return ApplyWithRotation(point, cos, sin);
		} else {
			return ApplyInverseWithRotation(point, cos, sin);
		}
	}

	template <TransformDirection Direction>
	[[nodiscard]] V2_float ApplyWithoutRotationImpl(V2_float point) const {
		if constexpr (Direction == TransformDirection::Forward) {
			return ApplyWithoutRotation(point);
		} else {
			return ApplyInverseWithoutRotation(point);
		}
	}

	template <TransformDirection Direction, typename TFunction>
	void WithPointTransform(TFunction&& function) const {
		if (HasRotation()) {
			float cos{ rotation_.Cos() };
			float sin{ rotation_.Sin() };

			auto transform = [this, cos, sin](V2_float point) {
				return ApplyWithRotationImpl<Direction>(point, cos, sin);
			};

			std::invoke(std::forward<TFunction>(function), transform);
			return;
		}

		if (!IsIdentity()) {
			auto transform = [this](V2_float point) {
				return ApplyWithoutRotationImpl<Direction>(point);
			};

			std::invoke(std::forward<TFunction>(function), transform);
			return;
		}

		auto transform = [](V2_float point) {
			return point;
		};

		std::invoke(std::forward<TFunction>(function), transform);
	}

	template <
		TransformDirection Direction, std::ranges::input_range TRange,
		InvocableR<V2_float, std::ranges::range_reference_t<TRange>> TGetPosition,
		InvocableR<void, std::ranges::range_reference_t<TRange>, V2_float> TSetPosition>
	void ApplyToElements(
		TRange&& elements, TGetPosition get_position, TSetPosition set_position
	) const {
		WithPointTransform<Direction>([&elements, &get_position, &set_position](auto&& transform) {
			for (auto&& element : elements) {
				auto position{ std::invoke(get_position, element) };
				std::invoke(set_position, element, transform(position));
			}
		});
	}

	V2_float position_;

	/// @brief Positive clockwise.
	Radians rotation_{ 0.0f };

	/// @brief Can be negative but not zero. Negative scale will flip the transform across the
	/// corresponding axis.
	V2_float scale_{ 1.0f, 1.0f };
};

} // namespace ptgn