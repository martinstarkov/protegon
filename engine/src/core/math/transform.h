#pragma once

#include <array>
#include <ostream>
#include <span>
#include <vector>

#include "core/math/tolerance.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "serialization/json/serialize.h"

namespace ptgn {

struct Transform {
	Transform() = default;

	template <Arithmetic T>
	Transform(const Vector2<T>& position) : position_{ position } {}

	Transform(V2_float position, float rotation, V2_float scale = { 1.0f, 1.0f });

	[[nodiscard]] Transform Inverse() const;

	[[nodiscard]] Transform RelativeTo(const Transform& parent) const;

	[[nodiscard]] Transform InverseRelativeTo(const Transform& parent) const;

	friend bool operator==(const Transform& a, const Transform& b) {
		return a.position_ == b.position_ && NearlyEqual(a.rotation_, b.rotation_) &&
			   a.scale_ == b.scale_;
	}

	[[nodiscard]] V2_float GetPosition() const;

	Transform& SetPosition(V2_float position);
	/// @brief Set position along a particular axis: x == 0, y == 1.
	Transform& SetPosition(std::size_t index, float position);
	Transform& SetPositionX(float x);
	Transform& SetPositionY(float y);

	/// @brief position += position_difference
	Transform& Translate(V2_float position_difference);
	Transform& TranslateX(float position_x_difference);
	Transform& TranslateY(float position_y_difference);

	/// @return Unit: Radians, Direction: Clockwise positive.
	[[nodiscard]] float GetRotation() const;

	/// @param rotation Unit: Radians, Direction: Clockwise positive.
	Transform& SetRotation(float rotation);

	/// @brief rotation += angle_difference
	/// @param angle_difference Unit: Radians, Direction: Clockwise positive.
	Transform& Rotate(float angle_difference);

	/// @brief Clamps rotation between [0, 2 pi).
	Transform& ClampRotation();

	/// @return (scale_x + scale_y) / 2
	[[nodiscard]] float GetAverageScale() const;

	[[nodiscard]] V2_float GetScale() const;

	Transform& SetScale(float scale);
	Transform& SetScale(V2_float scale);
	Transform& SetScaleX(float x);
	Transform& SetScaleY(float y);

	/// @brief scale *= scale_multiplier
	Transform& Scale(V2_float scale_multiplier);
	Transform& ScaleX(float scale_x_multiplier);
	Transform& ScaleY(float scale_y_multiplier);

	[[nodiscard]] V2_float Apply(V2_float point) const;

	[[nodiscard]] V2_float ApplyInverse(V2_float point) const;

	std::vector<V2_float> Apply(const std::vector<V2_float>& points) const;

	std::vector<V2_float> ApplyInverse(const std::vector<V2_float>& points) const;

	template <std::size_t N>
	std::array<V2_float, N> Apply(const std::array<V2_float, N>& points) const {
		std::array<V2_float, N> transformed_points;
		Apply(points, transformed_points);
		return transformed_points;
	}

	template <std::size_t N>
	std::array<V2_float, N> ApplyInverse(const std::array<V2_float, N>& points) const {
		std::array<V2_float, N> transformed_points;
		ApplyInverse(points, transformed_points);
		return transformed_points;
	}

	friend std::ostream& operator<<(std::ostream& os, const Transform& transform) {
		os << "{ position: ";
		os << transform.position_;
		os << ", rotation: ";
		os << transform.rotation_;
		os << ", scale: ";
		os << transform.scale_ << "}";
		return os;
	}

private:
	void Apply(std::span<const V2_float> points, std::span<V2_float> out_transformed_points) const;

	void ApplyInverse(std::span<const V2_float> points, std::span<V2_float> out_transformed_points)
		const;

	[[nodiscard]] V2_float ApplyWithRotation(
		V2_float point, float cos_angle_radians, float sin_angle_radians
	) const;

	[[nodiscard]] V2_float ApplyWithoutRotation(V2_float point) const;

	[[nodiscard]] V2_float ApplyInverseWithRotation(
		V2_float point, float cos_angle_radians, float sin_angle_radians
	) const;

	[[nodiscard]] V2_float ApplyInverseWithoutRotation(V2_float point) const;

	V2_float position_;

	/// @param rotation Unit: Radians, Direction: Clockwise positive.
	float rotation_{ 0.0f };

	/// @brief Can be negative but not zero. Negative scale will flip the transform across the
	/// corresponding axis.
	V2_float scale_{ 1.0f, 1.0f };

	PTGN_SERIALIZER_REGISTER_NAMED_IGNORE_DEFAULTS(
		Transform, KeyValue("position", position_), KeyValue("rotation", rotation_),
		KeyValue("scale", scale_)
	)
};

} // namespace ptgn