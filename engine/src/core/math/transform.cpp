#include "core/math/transform.h"

#include <algorithm>
#include <span>
#include <vector>

#include "core/assert.h"
#include "core/math/angle.h"
#include "core/math/vector2.h"

namespace ptgn {

Transform::Transform(V2_float position, Radians rotation, V2_float scale) :
	position_{ position }, rotation_{ rotation }, scale_{ scale } {}

Transform::Transform(V2_float position, Degrees rotation, V2_float scale) :
	Transform{ position, rotation.ToRad(), scale } {}

Transform Transform::Inverse() const {
	PTGN_ASSERT(!scale_.HasZero(), "Cannot get inverse of transform with zero");
	return { -position_, -rotation_, 1.0f / scale_ };
}

Transform Transform::RelativeTo(Transform parent) const {
	Transform result;
	// Order is important.
	result.scale_	 = parent.scale_ * scale_;
	result.rotation_ = parent.rotation_ + rotation_;
	result.position_ = parent.position_ + (parent.scale_ * position_).Rotated(parent.rotation_);
	return result;
}

Transform Transform::InverseRelativeTo(Transform parent) const {
	Transform local;

	auto inv_rotation{ -parent.rotation_ };
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
	// Absolute value applied because negative scale is used for flip.
	auto abs_scale{ Abs(scale_) };
	return (abs_scale.x + abs_scale.y) * 0.5f;
}

V2_float Transform::GetPosition() const {
	return position_;
}

bool Transform::HasRotation() const {
	return rotation_.value != 0.0f;
}

Degrees Transform::GetRotation() const {
	return rotation_.ToDeg();
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

Transform& Transform::SetRotation(Radians rotation) {
	rotation_ = rotation;
	return *this;
}

Transform& Transform::SetRotation(Degrees rotation) {
	return SetRotation(rotation.ToRad());
}

Transform& Transform::ClampRotation() {
	return SetRotation(Clamp(rotation_));
}

Transform& Transform::SetScale(float scale) {
	return SetScale(V2_float{ scale });
}

Transform& Transform::SetScale(V2_float scale) {
	scale_ = Clamp(scale, kMinScale, kMaxScale);
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

Transform& Transform::Rotate(Radians angle_difference) {
	return SetRotation(rotation_ + angle_difference);
}

Transform& Transform::Rotate(Degrees angle_difference) {
	return Rotate(angle_difference.ToRad());
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

V2_float Transform::ApplyWithRotation(V2_float point, float cos, float sin) const {
	PTGN_ASSERT(!scale_.HasZero(), "Cannot transform point for an object with zero ");
	return position_ + (scale_ * point).Rotated(cos, sin);
}

V2_float Transform::ApplyWithoutRotation(V2_float point) const {
	PTGN_ASSERT(!scale_.HasZero(), "Cannot transform point for an object with zero");
	return position_ + scale_ * point;
}

V2_float Transform::ApplyInverseWithRotation(V2_float point, float cos, float sin) const {
	PTGN_ASSERT(!scale_.HasZero(), "Cannot inverse transform point for an object with zero");

	return (point - position_).Rotated(cos, -sin) / scale_;
}

V2_float Transform::ApplyInverseWithoutRotation(V2_float point) const {
	PTGN_ASSERT(!scale_.HasZero(), "Cannot inverse transform point for an object with zero");

	return (point - position_) / scale_;
}

V2_float Transform::Apply(V2_float point) const {
	if (HasRotation()) {
		return ApplyWithRotation(point, rotation_.Cos(), rotation_.Sin());
	}
	if (*this != Transform{}) {
		return ApplyWithoutRotation(point);
	}
	return point;
}

V2_float Transform::ApplyInverse(V2_float point) const {
	if (HasRotation()) {
		return ApplyInverseWithRotation(point, rotation_.Cos(), rotation_.Sin());
	}
	if (*this != Transform{}) {
		return ApplyInverseWithoutRotation(point);
	}
	return point;
}

void Transform::Apply(std::span<const V2_float> points, std::span<V2_float> out_transformed_points)
	const {
	PTGN_ASSERT(out_transformed_points.size() >= points.size());

	if (HasRotation()) {
		float cos{ rotation_.Cos() };
		float sin{ rotation_.Sin() };

		for (std::size_t i{ 0 }; i < points.size(); ++i) {
			out_transformed_points[i] = ApplyWithRotation(points[i], cos, sin);
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

	if (HasRotation()) {
		float cos{ rotation_.Cos() };
		float sin{ rotation_.Sin() };

		for (std::size_t i{ 0 }; i < points.size(); ++i) {
			out_transformed_points[i] = ApplyInverseWithRotation(points[i], cos, sin);
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