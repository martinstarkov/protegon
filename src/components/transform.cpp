#include "components/transform.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "components/offsets.h"
#include "core/entity.h"
#include "core/entity_hierarchy.h"
#include "math/math.h"
#include "math/vector2.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "utility/flags.h"

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

Transform::Transform(const V2_float& position) : position_{ position } {}

Transform::Transform(const V2_float& position, float rotation, const V2_float& scale) :
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

Transform& Transform::SetPosition(const V2_float& position) {
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

Transform& Transform::SetScale(const V2_float& scale) {
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

Transform& Transform::Translate(const V2_float& position_difference) {
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

Transform& Transform::Scale(const V2_float& scale_multiplier) {
	return SetScale(scale_ * scale_multiplier);
}

Transform& Transform::ScaleX(float scale_x_multiplier) {
	return SetScaleX(scale_.x * scale_x_multiplier);
}

Transform& Transform::ScaleY(float scale_y_multiplier) {
	return SetScaleY(scale_.y * scale_y_multiplier);
}

bool Transform::IsPositionDirty() const {
	return dirty_flags_.IsSet(impl::TransformDirty::Position);
}

bool Transform::IsRotationDirty() const {
	return dirty_flags_.IsSet(impl::TransformDirty::Rotation);
}

bool Transform::IsScaleDirty() const {
	return dirty_flags_.IsSet(impl::TransformDirty::Scale);
}

bool Transform::IsDirty() const {
	return dirty_flags_.AnySet();
}

void Transform::ClearDirtyFlags() const {
	dirty_flags_.ClearAll();
}

V2_float Transform::ApplyWithRotation(
	const V2_float& point, float cos_angle_radians, float sin_angle_radians
) const {
	PTGN_ASSERT(!scale_.IsZero(), "Cannot transform point for an object with zero scale");
	return position_ + (scale_ * point).Rotated(cos_angle_radians, sin_angle_radians);
}

V2_float Transform::ApplyWithoutRotation(const V2_float& point) const {
	PTGN_ASSERT(!scale_.IsZero(), "Cannot transform point for an object with zero scale");
	return position_ + scale_ * point;
}

V2_float Transform::ApplyInverseWithRotation(
	const V2_float& point, float cos_angle_radians, float sin_angle_radians
) const {
	PTGN_ASSERT(!scale_.IsZero(), "Cannot inverse transform point for an object with zero scale");

	return (point - position_).Rotated(cos_angle_radians, -sin_angle_radians) / scale_;
}

V2_float Transform::ApplyInverseWithoutRotation(const V2_float& point) const {
	PTGN_ASSERT(!scale_.IsZero(), "Cannot inverse transform point for an object with zero scale");

	return (point - position_) / scale_;
}

V2_float Transform::Apply(const V2_float& point) const {
	if (rotation_ != 0.0f) {
		return ApplyWithRotation(point, std::cos(rotation_), std::sin(rotation_));
	}
	if (*this != Transform{}) {
		return ApplyWithoutRotation(point);
	}
	return point;
}

V2_float Transform::ApplyInverse(const V2_float& point) const {
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

Camera& SetTransform(Camera& entity, const Transform& transform) {
	entity.SetScroll(transform.GetPosition());
	entity.SetRotation(transform.GetRotation());
	entity.SetZoom(transform.GetScale());
	return entity;
}

Transform& GetTransform(Entity& entity) {
	PTGN_ASSERT(
		!entity.Has<impl::CameraInstance>(),
		"GetTransform(Entity) cannot be used on a Camera entity. Use "
		"camera.GetTransform() instead."
	);
	return impl::EntityAccess::TryAdd<Transform>(entity);
}

Transform GetTransform(const Entity& entity) {
	PTGN_ASSERT(
		!entity.Has<impl::CameraInstance>(),
		"GetTransform(Entity) cannot be used on a Camera entity. Use "
		"GetTransform(Camera) instead."
	);
	if (auto transform{ entity.TryGet<Transform>() }; transform) {
		return *transform;
	}
	return {};
}

Transform GetTransform(const Camera& camera) {
	return { camera.GetScroll(), camera.GetRotation(), camera.GetZoom() };
}

Transform GetAbsoluteTransform(const Entity& entity) {
	Transform world_transform;
	auto transform{ GetTransform(entity) };
	if (entity.Has<impl::IgnoreParentTransform>() && entity.Get<impl::IgnoreParentTransform>()) {
		world_transform = transform;
	} else {
		Transform relative_to;
		if (HasParent(entity)) {
			Entity parent{ GetParent(entity) };
			relative_to = GetAbsoluteTransform(parent);
		}
		world_transform = transform.RelativeTo(relative_to);
	}
	if (const auto camera{ entity.GetNonPrimaryCamera() }) {
		auto camera_transform{ GetTransform(*camera) };
		auto scale{ camera_transform.GetScale() };
		auto camera_scale{ entity.GetScene().GetCameraScaleRelativeTo(*camera) };
		PTGN_ASSERT(camera_scale.BothAboveZero());
		scale /= camera_scale;
		camera_transform.SetScale(scale);
		auto inverse_transform{ world_transform.InverseRelativeTo(camera_transform) };
		auto primary_camera{ entity.GetScene().camera };
		auto primary_transform{ GetTransform(primary_camera) };
		auto absolute_transform{ inverse_transform.RelativeTo(primary_transform) };
		return absolute_transform;
	}
	return world_transform;
}

Transform GetAbsoluteTransform(const Camera& entity) {
	auto world_transform{ GetWorldTransform(entity) };
	return world_transform;
}

Transform GetWorldTransform(const Entity& entity) {
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

Transform GetWorldTransform(const Camera& entity) {
	auto transform{ GetTransform(entity) };
	return transform;
}

Transform GetDrawTransform(const Entity& entity) {
	auto offset_transform{ GetOffset(entity) };
	PTGN_ASSERT(!entity.Has<impl::CameraInstance>());
	auto transform{ GetWorldTransform(entity) };
	transform = transform.RelativeTo(offset_transform);
	return transform;
}

} // namespace ptgn