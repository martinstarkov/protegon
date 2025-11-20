#include "ecs/components/transform.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/util/flags.h"
#include "ecs/entity.h"
#include "ecs/entity_hierarchy.h"
#include "math/math_utils.h"
#include "math/vector2.h"
#include "scene/scene.h"

namespace ptgn {

Transform::Transform(const Transform& other) {
	*this = other;
}

Transform& Transform::operator=(const Transform& other) {
	if (&other != this) {
		SetPosition(other.position_);
		SetRotation(other.rotation_);
		SetScale(other.scale_);
	}
	return *this;
}

Transform::Transform(Transform&& other) noexcept :
	position_{ std::exchange(other.position_, {}) },
	rotation_{ std::exchange(other.rotation_, 0.0f) },
	scale_{ std::exchange(other.scale_, {}) },
	dirty_flags_{ impl::TransformDirty::Position | impl::TransformDirty::Rotation |
				  impl::TransformDirty::Scale } {}

Transform& Transform::operator=(Transform&& other) noexcept {
	if (&other != this) {
		SetPosition(other.position_);
		SetRotation(other.rotation_);
		SetScale(other.scale_);
		other.position_ = {};
		other.rotation_ = 0.0f;
		other.scale_	= {};
	}
	return *this;
}

Transform::Transform(V2_float position, float rotation, V2_float scale) :
	position_{ position }, rotation_{ rotation }, scale_{ scale } {}

Transform Transform::Inverse() const {
	PTGN_ASSERT(!scale_.HasZero(), "Cannot get inverse of transform with zero scale");
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
	if (position_ == position) {
		return *this;
	}
	position_ = position;
	dirty_flags_.Set(impl::TransformDirty::Position);
	return *this;
}

Transform& Transform::SetPositionX(float x) {
	return SetPosition(V2_float{ x, position_.y });
}

Transform& Transform::SetPositionY(float y) {
	return SetPosition(V2_float{ position_.x, y });
}

Transform& Transform::SetRotation(float rotation) {
	if (rotation_ == rotation) {
		return *this;
	}
	rotation_ = rotation;
	dirty_flags_.Set(impl::TransformDirty::Rotation);
	return *this;
}

Transform& Transform::ClampRotation() {
	return SetRotation(ClampAngle2Pi(rotation_));
}

Transform& Transform::SetScale(float scale) {
	return SetScale(V2_float{ scale });
}

Transform& Transform::SetScale(V2_float scale) {
	PTGN_ASSERT(!scale.HasZero(), "Cannot set transform scale with a zero component");
	if (scale_ == scale) {
		return *this;
	}
	scale_ = scale;
	dirty_flags_.Set(impl::TransformDirty::Scale);
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

bool Transform::IsDirty() const {
	return dirty_flags_.AnySet();
}

void Transform::ClearDirtyFlags() const {
	dirty_flags_.ClearAll();
}

V2_float Transform::ApplyWithRotation(
	V2_float point, float cos_angle_radians, float sin_angle_radians
) const {
	PTGN_ASSERT(!scale_.IsZero(), "Cannot transform point for an object with zero scale");
	return position_ + (scale_ * point).Rotated(cos_angle_radians, sin_angle_radians);
}

V2_float Transform::ApplyWithoutRotation(V2_float point) const {
	PTGN_ASSERT(!scale_.IsZero(), "Cannot transform point for an object with zero scale");
	return position_ + scale_ * point;
}

V2_float Transform::ApplyInverseWithRotation(
	V2_float point, float cos_angle_radians, float sin_angle_radians
) const {
	PTGN_ASSERT(!scale_.IsZero(), "Cannot inverse transform point for an object with zero scale");

	return (point - position_).Rotated(cos_angle_radians, -sin_angle_radians) / scale_;
}

V2_float Transform::ApplyInverseWithoutRotation(V2_float point) const {
	PTGN_ASSERT(!scale_.IsZero(), "Cannot inverse transform point for an object with zero scale");

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

Entity SetTransform(Entity entity, const Transform& transform) {
	entity.template Add<Transform>(transform);
	return entity;
}

Transform GetTransform(Entity entity) {
	return entity.template TryAdd<Transform>();
}

Transform GetWorldTransform(Entity entity) {
	auto transform{ GetTransform(entity) };
	if (entity.Has<impl::IgnoreParentTransform>() && entity.Get<impl::IgnoreParentTransform>()) {
		return transform;
	}
	Transform relative_to;
	if (HasParent(entity)) {
		Entity parent{ GetParent(entity) };
		relative_to = GetWorldTransform(parent);
	}
	auto world_transform{ transform.RelativeTo(relative_to) };
	return world_transform;
}

V2_float GetPosition(Entity entity) {
	return GetTransform(entity).GetPosition();
}

V2_float GetWorldPosition(Entity entity) {
	return GetWorldTransform(entity).GetPosition();
}

float GetRotation(Entity entity) {
	return GetTransform(entity).GetRotation();
}

float GetWorldRotation(Entity entity) {
	return GetWorldTransform(entity).GetRotation();
}

V2_float GetScale(Entity entity) {
	return GetTransform(entity).GetScale();
}

V2_float GetWorldScale(Entity entity) {
	return GetWorldTransform(entity).GetScale();
}

Entity SetPosition(Entity entity, V2_float position) {
	auto transform{ GetTransform(entity) };
	transform.SetPosition(position);
	return SetTransform(entity, transform);
}

Entity SetPositionX(Entity entity, float position_x) {
	return SetPosition(entity, V2_float{ position_x, GetPosition(entity).y });
}

Entity SetPositionY(Entity entity, float position_y) {
	return SetPosition(entity, V2_float{ GetPosition(entity).x, position_y });
}

Entity Translate(Entity entity, V2_float position_difference) {
	return SetPosition(entity, GetPosition(entity) + position_difference);
}

Entity TranslateX(Entity entity, float position_x_difference) {
	return Translate(entity, V2_float{ position_x_difference, 0.0f });
}

Entity TranslateY(Entity entity, float position_y_difference) {
	return Translate(entity, V2_float{ 0.0f, position_y_difference });
}

Entity SetRotation(Entity entity, float rotation) {
	auto transform{ GetTransform(entity) };
	transform.SetRotation(rotation);
	return SetTransform(entity, transform);
}

Entity Rotate(Entity entity, float angle_difference) {
	return SetRotation(entity, GetRotation(entity) + angle_difference);
}

Entity SetScale(Entity entity, V2_float scale) {
	auto transform{ GetTransform(entity) };
	transform.SetScale(scale);
	return SetTransform(entity, transform);
}

Entity SetScale(Entity entity, float scale) {
	return SetScale(entity, V2_float{ scale });
}

Entity SetScaleX(Entity entity, float scale_x) {
	return SetScale(entity, V2_float{ scale_x, GetScale(entity).y });
}

Entity SetScaleY(Entity entity, float scale_y) {
	return SetScale(entity, V2_float{ GetScale(entity).x, scale_y });
}

Entity Scale(Entity entity, V2_float scale_multiplier) {
	return SetScale(entity, GetScale(entity) * scale_multiplier);
}

Entity ScaleX(Entity entity, float scale_x_multiplier) {
	V2_float scale{ GetScale(entity) };
	scale.x *= scale_x_multiplier;
	return SetScale(entity, scale);
}

Entity ScaleY(Entity entity, float scale_y_multiplier) {
	V2_float scale{ GetScale(entity) };
	scale.y *= scale_y_multiplier;
	return SetScale(entity, scale);
}

} // namespace ptgn