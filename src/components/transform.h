#pragma once

#include <cstdint>

#include "components/generic.h"
#include "core/entity.h"
#include "math/vector2.h"
#include "serialization/serializable.h"
#include "utility/flags.h"

namespace ptgn {

class Camera;
class Scene;

namespace impl {

enum class TransformDirty : std::uint8_t {
	None	 = 0,
	Position = 1 << 0,
	Rotation = 1 << 1,
	Scale	 = 1 << 2,
};

PTGN_FLAGS_OPERATORS(TransformDirty)

struct IgnoreParentTransform : public BoolComponent {
	using BoolComponent::BoolComponent;

	IgnoreParentTransform() : BoolComponent{ true } {}
};

} // namespace impl

struct Transform {
	Transform()			  = default;
	~Transform() noexcept = default;
	Transform(Transform&&) noexcept;
	Transform& operator=(Transform&&) noexcept;
	Transform(const Transform& other);
	Transform& operator=(const Transform& other);

	Transform(const V2_float& position);

	Transform(const V2_float& position, float rotation, const V2_float& scale = { 1.0f, 1.0f });

	[[nodiscard]] Transform RelativeTo(const Transform& parent) const;

	[[nodiscard]] Transform InverseRelativeTo(const Transform& parent) const;

	[[nodiscard]] float GetAverageScale() const;

	// Dirty flags should not be compared.
	friend bool operator==(const Transform& a, const Transform& b) {
		return a.position_ == b.position_ && NearlyEqual(a.rotation_, b.rotation_) &&
			   a.scale_ == b.scale_;
	}

	[[nodiscard]] V2_float GetPosition() const;
	[[nodiscard]] float GetRotation() const;
	[[nodiscard]] V2_float GetScale() const;

	Transform& SetPosition(const V2_float& position);
	// Set position along a particular axis: x == 0, y == 1.
	Transform& SetPosition(std::size_t index, float position);
	Transform& SetPositionX(float x);
	Transform& SetPositionY(float y);
	Transform& SetRotation(float rotation);
	Transform& SetScale(float scale);
	Transform& SetScale(const V2_float& scale);
	Transform& SetScaleX(float x);
	Transform& SetScaleY(float y);

	// position += position_difference
	Transform& Translate(const V2_float& position_difference);
	Transform& TranslateX(float position_x_difference);
	Transform& TranslateY(float position_y_difference);

	// rotation += angle_difference
	Transform& Rotate(float angle_difference);

	// scale *= scale_multiplier
	Transform& Scale(const V2_float& scale_multiplier);
	Transform& ScaleX(float scale_x_multiplier);
	Transform& ScaleY(float scale_y_multiplier);

	// Clamps rotation between [0, 2 pi).
	Transform& ClampRotation();

	[[nodiscard]] bool IsPositionDirty() const;
	[[nodiscard]] bool IsRotationDirty() const;
	[[nodiscard]] bool IsScaleDirty() const;
	[[nodiscard]] bool IsDirty() const;
	void ClearDirtyFlags() const;

private:
	friend class Scene;

	V2_float position_;
	float rotation_{ 0.0f };
	V2_float scale_{ 1.0f, 1.0f };

	// By default all flags are dirty.
	mutable Flags<impl::TransformDirty> dirty_flags_{ impl::TransformDirty::Position |
													  impl::TransformDirty::Rotation |
													  impl::TransformDirty::Scale };

	PTGN_SERIALIZER_REGISTER_NAMED_IGNORE_DEFAULTS(
		Transform, KeyValue("position", position_), KeyValue("rotation", rotation_),
		KeyValue("scale", scale_), KeyValue("dirty_flags", dirty_flags_)
	)
};

// Set the transform of the entity with respect to its parent entity.
// @return *this.
Entity& SetTransform(Entity& entity, const Transform& transform);
Camera& SetTransform(Camera& entity, const Transform& transform);

// @return The transform of the entity with respect to its parent entity.
[[nodiscard]] Transform GetTransform(const Entity& entity);
[[nodiscard]] Transform& GetTransform(Entity& entity);

[[nodiscard]] Transform GetTransform(const Camera& camera);
[[nodiscard]] Transform& GetTransform(Camera& camera);

// @return The transform of the entity with respect to the scene primary camera.
[[nodiscard]] Transform GetAbsoluteTransform(const Entity& entity);
[[nodiscard]] Transform GetAbsoluteTransform(const Camera& entity);

// @return The transform of the entity with respect to its parent camera.
[[nodiscard]] Transform GetWorldTransform(const Entity& entity);
[[nodiscard]] Transform GetWorldTransform(const Camera& entity);

// @return The transform of the entity with respect to its parent camera, including any visual
// offsets caused by effects such as shake or bounce.
[[nodiscard]] Transform GetDrawTransform(const Entity& entity);

template <EntityBase T>
T& SetPosition(T& entity, const V2_float& position) {
	GetTransform(entity).SetPosition(position);
	return entity;
}

template <EntityBase T>
T& SetPositionX(T& entity, float position_x) {
	GetTransform(entity).SetPositionX(position_x);
	return entity;
}

template <EntityBase T>
T& SetPositionY(T& entity, float position_y) {
	GetTransform(entity).SetPositionY(position_y);
	return entity;
}

template <EntityBase T>
T& Translate(T& entity, const V2_float& position_difference) {
	GetTransform(entity).Translate(position_difference);
	return entity;
}

template <EntityBase T>
T& TranslateX(T& entity, float position_x_difference) {
	GetTransform(entity).TranslateX(position_x_difference);
	return entity;
}

template <EntityBase T>
T& TranslateY(T& entity, float position_y_difference) {
	GetTransform(entity).TranslateY(position_y_difference);
	return entity;
}

template <EntityBase T>
V2_float GetPosition(const T& entity) {
	return GetTransform(entity).GetPosition();
}

template <EntityBase T>
V2_float GetAbsolutePosition(const T& entity) {
	return GetAbsoluteTransform(entity).GetPosition();
}

// Set 2D rotation angle in radians.
/* Range: (-3.14159, 3.14159].
 * (clockwise positive).
 *            -1.5708
 *               |
 *    3.14159 ---o--- 0
 *               |
 *             1.5708
 */
template <EntityBase T>
T& SetRotation(T& entity, float rotation) {
	GetTransform(entity).SetRotation(rotation);
	return entity;
}

template <EntityBase T>
T& Rotate(T& entity, float angle_difference) {
	GetTransform(entity).Rotate(angle_difference);
	return entity;
}

template <EntityBase T>
float GetRotation(const T& entity) {
	return GetTransform(entity).GetRotation();
}

template <EntityBase T>
float GetAbsoluteRotation(const T& entity) {
	return GetAbsoluteTransform(entity).GetRotation();
}

template <EntityBase T>
T& SetScale(T& entity, float scale) {
	return SetScale(entity, V2_float{ scale });
}

template <EntityBase T>
T& SetScale(T& entity, const V2_float& scale) {
	GetTransform(entity).SetScale(scale);
	return entity;
}

template <EntityBase T>
T& SetScaleX(T& entity, float scale_x) {
	GetTransform(entity).SetScaleX(scale_x);
	return entity;
}

template <EntityBase T>
T& SetScaleY(T& entity, float scale_y) {
	GetTransform(entity).SetScaleY(scale_y);
	return entity;
}

template <EntityBase T>
T& Scale(T& entity, const V2_float& scale_multiplier) {
	GetTransform(entity).Scale(scale_multiplier);
	return entity;
}

template <EntityBase T>
T& ScaleX(T& entity, float scale_x_multiplier) {
	GetTransform(entity).ScaleX(scale_x_multiplier);
	return entity;
}

template <EntityBase T>
T& ScaleY(T& entity, float scale_y_multiplier) {
	GetTransform(entity).ScaleY(scale_y_multiplier);
	return entity;
}

template <EntityBase T>
V2_float GetScale(const T& entity) {
	return GetTransform(entity).GetScale();
}

template <EntityBase T>
V2_float GetAbsoluteScale(const T& entity) {
	return GetAbsoluteTransform(entity).GetScale();
}

} // namespace ptgn