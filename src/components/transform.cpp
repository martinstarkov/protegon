#include "components/transform.h"

#include <utility>

#include "common/assert.h"
#include "components/offsets.h"
#include "core/entity.h"
#include "core/entity_hierarchy.h"
#include "math/math.h"
#include "math/vector2.h"
#include "renderer/api/origin.h"
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

Entity& SetTransform(Entity& entity, const Transform& transform) {
	if (entity.Has<Transform>()) {
		impl::EntityAccess::Get<Transform>(entity) = transform;
	} else {
		impl::EntityAccess::Add<Transform>(entity, transform);
	}
	return entity;
}

Transform& GetTransform(Entity& entity) {
	return impl::EntityAccess::TryAdd<Transform>(entity);
}

Transform GetTransform(const Entity& entity) {
	if (auto transform{ entity.TryGet<Transform>() }; transform) {
		return *transform;
	}
	return {};
}

Transform GetAbsoluteTransform(const Entity& entity, bool relative_to_scene_primary_camera) {
	auto transform{ GetTransform(entity) };
	if (entity.Has<impl::IgnoreParentTransform>() && entity.Get<impl::IgnoreParentTransform>()) {
		return transform;
	}
	Transform relative_to;
	if (HasParent(entity)) {
		Entity parent{ GetParent(entity) };
		relative_to = GetAbsoluteTransform(parent, relative_to_scene_primary_camera);
	}
	auto absolute_transform{ transform.RelativeTo(relative_to) };
	if (!relative_to_scene_primary_camera || entity.Has<impl::CameraInfo>()) {
		return absolute_transform;
	}
	if (const auto camera{ entity.GetNonPrimaryCamera() }) {
		auto camera_transform{ GetTransform(*camera) };
		return absolute_transform.RelativeTo(camera_transform);
	}
	return absolute_transform;
}

Transform GetDrawTransform(const Entity& entity) {
	auto offset_transform{ GetOffset(entity) };
	auto transform{ GetAbsoluteTransform(entity, false) };
	transform = transform.RelativeTo(offset_transform);
	return transform;
}

Entity& SetPosition(Entity& entity, const V2_float& position) {
	GetTransform(entity).SetPosition(position);
	return entity;
}

Entity& SetPositionX(Entity& entity, float position_x) {
	GetTransform(entity).SetPositionX(position_x);
	return entity;
}

Entity& SetPositionY(Entity& entity, float position_y) {
	GetTransform(entity).SetPositionY(position_y);
	return entity;
}

Entity& Translate(Entity& entity, const V2_float& position_difference) {
	GetTransform(entity).Translate(position_difference);
	return entity;
}

Entity& TranslateX(Entity& entity, float position_x_difference) {
	GetTransform(entity).TranslateX(position_x_difference);
	return entity;
}

Entity& TranslateY(Entity& entity, float position_y_difference) {
	GetTransform(entity).TranslateY(position_y_difference);
	return entity;
}

V2_float GetPosition(const Entity& entity) {
	return GetTransform(entity).GetPosition();
}

V2_float GetAbsolutePosition(const Entity& entity) {
	return GetAbsoluteTransform(entity).GetPosition();
}

Entity& SetRotation(Entity& entity, float rotation) {
	GetTransform(entity).SetRotation(rotation);
	return entity;
}

Entity& Rotate(Entity& entity, float angle_difference) {
	GetTransform(entity).Rotate(angle_difference);
	return entity;
}

float GetRotation(const Entity& entity) {
	return GetTransform(entity).GetRotation();
}

float GetAbsoluteRotation(const Entity& entity) {
	return GetAbsoluteTransform(entity).GetRotation();
}

Entity& SetScale(Entity& entity, float scale) {
	return SetScale(entity, V2_float{ scale });
}

Entity& SetScale(Entity& entity, const V2_float& scale) {
	GetTransform(entity).SetScale(scale);
	return entity;
}

Entity& SetScaleX(Entity& entity, float scale_x) {
	GetTransform(entity).SetScaleX(scale_x);
	return entity;
}

Entity& SetScaleY(Entity& entity, float scale_y) {
	GetTransform(entity).SetScaleY(scale_y);
	return entity;
}

Entity& Scale(Entity& entity, const V2_float& scale_multiplier) {
	GetTransform(entity).Scale(scale_multiplier);
	return entity;
}

Entity& ScaleX(Entity& entity, float scale_x_multiplier) {
	GetTransform(entity).ScaleX(scale_x_multiplier);
	return entity;
}

Entity& ScaleY(Entity& entity, float scale_y_multiplier) {
	GetTransform(entity).ScaleY(scale_y_multiplier);
	return entity;
}

V2_float GetScale(const Entity& entity) {
	return GetTransform(entity).GetScale();
}

V2_float GetAbsoluteScale(const Entity& entity) {
	return GetAbsoluteTransform(entity).GetScale();
}

} // namespace ptgn