#include "core/math/transform.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

#include "core/assert.h"
#include "core/math/math_utils.h"
#include "core/math/vector2.h"

namespace ptgn {

Transform::Transform(V2_float position, float rotation, V2_float scale) :
	position_{ position }, rotation_{ rotation }, scale_{ scale } {}

Transform Transform::Inverse() const {
	PTGN_ASSERT(
		scale_.BothAboveZero(), "Cannot get inverse of transform with zero or negative scale"
	);
	return { -position_, -rotation_, 1.0f / scale_ };
}

Transform Transform::RelativeTo(const Transform& parent) const {
	Transform result;
	// Order is important.
	result.scale_	 = parent.scale_ * scale_;
	result.rotation_ = parent.rotation_ + rotation_;
	result.position_ = parent.position_ + (parent.scale_ * position_).Rotated(parent.rotation_);
	return result;
}

Transform Transform::InverseRelativeTo(const Transform& parent) const {
	Transform local;

	float inv_rotation{ -parent.rotation_ };
	V2_float inv_scale{ parent.scale_.x != 0 ? 1.0f / parent.scale_.x : 0.0f,
						parent.scale_.y != 0 ? 1.0f / parent.scale_.y : 0.0f };

	V2_float delta{ position_ - parent.position_ };

	// Unrotate and unscale the position.
	local.position_	 = delta.Rotated(inv_rotation);
	local.position_ *= inv_scale;

	local.rotation_ = rotation_ - parent.rotation_;
	local.scale_	= scale_ * inv_scale;

	return local;
}

float Transform::GetAverageScale() const {
	// Abs because scale is used for flip.
	return (Abs(scale_.x) + Abs(scale_.y)) * 0.5f;
}

V2_float Transform::GetPosition() const {
	return position_;
}

float Transform::GetRotation() const {
	return rotation_;
}

V2_float Transform::GetScale() const {
	return scale_;
}

Transform& Transform::SetPosition(std::size_t index, float position) {
	PTGN_ASSERT(index == 0 || index == 1, "Axis index out of range");
	if (index == 0) {
		return SetPositionX(position);
	}
	return SetPositionY(position);
}

Transform& Transform::SetPosition(V2_float position) {
	position_ = position;
	return *this;
}

Transform& Transform::SetPositionX(float x) {
	return SetPosition(V2_float{ x, position_.y });
}

Transform& Transform::SetPositionY(float y) {
	return SetPosition(V2_float{ position_.x, y });
}

Transform& Transform::SetRotation(float rotation) {
	rotation_ = rotation;
	return *this;
}

Transform& Transform::ClampRotation() {
	return SetRotation(ClampAngle2Pi(rotation_));
}

Transform& Transform::SetScale(float scale) {
	return SetScale(V2_float{ scale });
}

Transform& Transform::SetScale(V2_float scale) {
	PTGN_ASSERT(scale.BothAboveZero(), "Cannot set transform scale to be zero or negative");
	scale_ = scale;
	return *this;
}

Transform& Transform::SetScaleX(float x) {
	return SetScale(V2_float{ x, scale_.y });
}

Transform& Transform::SetScaleY(float y) {
	return SetScale(V2_float{ scale_.x, y });
}

Transform& Transform::Translate(V2_float position_difference) {
	return SetPosition(position_ + position_difference);
}

Transform& Transform::TranslateX(float position_x_difference) {
	return SetPositionX(position_.x + position_x_difference);
}

Transform& Transform::TranslateY(float position_y_difference) {
	return SetPositionX(position_.y + position_y_difference);
}

Transform& Transform::Rotate(float angle_difference) {
	return SetRotation(rotation_ + angle_difference);
}

Transform& Transform::Scale(V2_float scale_multiplier) {
	return SetScale(scale_ * scale_multiplier);
}

Transform& Transform::ScaleX(float scale_x_multiplier) {
	return SetScaleX(scale_.x * scale_x_multiplier);
}

Transform& Transform::ScaleY(float scale_y_multiplier) {
	return SetScaleY(scale_.y * scale_y_multiplier);
}

V2_float Transform::ApplyWithRotation(
	V2_float point, float cos_angle_radians, float sin_angle_radians
) const {
	PTGN_ASSERT(
		scale_.BothAboveZero(), "Cannot transform point for an object with zero or negative scale"
	);
	return position_ + (scale_ * point).Rotated(cos_angle_radians, sin_angle_radians);
}

V2_float Transform::ApplyWithoutRotation(V2_float point) const {
	PTGN_ASSERT(
		scale_.BothAboveZero(), "Cannot transform point for an object with zero or negative scale"
	);
	return position_ + scale_ * point;
}

V2_float Transform::ApplyInverseWithRotation(
	V2_float point, float cos_angle_radians, float sin_angle_radians
) const {
	PTGN_ASSERT(
		scale_.BothAboveZero(),
		"Cannot inverse transform point for an object with zero or negative scale"
	);

	return (point - position_).Rotated(cos_angle_radians, -sin_angle_radians) / scale_;
}

V2_float Transform::ApplyInverseWithoutRotation(V2_float point) const {
	PTGN_ASSERT(
		scale_.BothAboveZero(),
		"Cannot inverse transform point for an object with zero or negative scale"
	);

	return (point - position_) / scale_;
}

V2_float Transform::Apply(V2_float point) const {
	if (rotation_ != 0.0f) {
		return ApplyWithRotation(point, std::cos(rotation_), std::sin(rotation_));
	}
	if (*this != Transform{}) {
		return ApplyWithoutRotation(point);
	}
	return point;
}

V2_float Transform::ApplyInverse(V2_float point) const {
	if (rotation_ != 0.0f) {
		return ApplyInverseWithRotation(point, std::cos(rotation_), std::sin(rotation_));
	}
	if (*this != Transform{}) {
		return ApplyInverseWithoutRotation(point);
	}
	return point;
}

void Transform::Apply(std::span<const V2_float> points, std::span<V2_float> out_transformed_points)
	const {
	PTGN_ASSERT(out_transformed_points.size() >= points.size());

	if (rotation_ != 0.0f) {
		float cosA{ std::cos(rotation_) };
		float sinA{ std::sin(rotation_) };

		for (std::size_t i{ 0 }; i < points.size(); ++i) {
			out_transformed_points[i] = ApplyWithRotation(points[i], cosA, sinA);
		}
		return;
	}

	if (*this != Transform{}) {
		for (std::size_t i{ 0 }; i < points.size(); ++i) {
			out_transformed_points[i] = ApplyWithoutRotation(points[i]);
		}
		return;
	}

	std::ranges::copy(points, out_transformed_points.begin());
}

void Transform::ApplyInverse(
	std::span<const V2_float> points, std::span<V2_float> out_transformed_points
) const {
	PTGN_ASSERT(out_transformed_points.size() >= points.size());

	if (rotation_ != 0.0f) {
		float cosA{ std::cos(rotation_) };
		float sinA{ std::sin(rotation_) };

		for (std::size_t i{ 0 }; i < points.size(); ++i) {
			out_transformed_points[i] = ApplyInverseWithRotation(points[i], cosA, sinA);
		}
		return;
	}

	if (*this != Transform{}) {
		for (std::size_t i{ 0 }; i < points.size(); ++i) {
			out_transformed_points[i] = ApplyInverseWithoutRotation(points[i]);
		}
		return;
	}

	std::ranges::copy(points, out_transformed_points.begin());
}

std::vector<V2_float> Transform::Apply(const std::vector<V2_float>& points) const {
	std::vector<V2_float> transformed_points(points.size());
	Apply(points, transformed_points);
	return transformed_points;
}

std::vector<V2_float> Transform::ApplyInverse(const std::vector<V2_float>& points) const {
	std::vector<V2_float> transformed_points(points.size());
	ApplyInverse(points, transformed_points);
	return transformed_points;
}

} // namespace ptgn