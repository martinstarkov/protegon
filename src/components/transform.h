#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "components/generic.h"
#include "core/entity.h"
#include "math/tolerance.h"
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

	[[nodiscard]] Transform Inverse() const;

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

	[[nodiscard]] V2_float Apply(const V2_float& point) const;

	[[nodiscard]] V2_float ApplyInverse(const V2_float& point) const;

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

private:
	friend class Scene;

	void Apply(std::span<const V2_float> points, std::span<V2_float> out_transformed_points) const;

	void ApplyInverse(std::span<const V2_float> points, std::span<V2_float> out_transformed_points)
		const;

	[[nodiscard]] V2_float ApplyWithRotation(
		const V2_float& point, float cos_angle_radians, float sin_angle_radians
	) const;

	[[nodiscard]] V2_float ApplyWithoutRotation(const V2_float& point) const;

	[[nodiscard]] V2_float ApplyInverseWithRotation(
		const V2_float& point, float cos_angle_radians, float sin_angle_radians
	) const;

	[[nodiscard]] V2_float ApplyInverseWithoutRotation(const V2_float& point) const;

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
template <EntityBase T>
T& SetTransform(T& entity, const Transform& transform) {
	if (entity.template Has<Transform>()) {
		impl::EntityAccess::Get<Transform>(entity) = transform;
	} else {
		impl::EntityAccess::Add<Transform>(entity, transform);
	}
	return entity;
}

Camera& SetTransform(Camera& entity, const Transform& transform);

// @return The transform of the entity with respect to its parent entity.
[[nodiscard]] Transform GetTransform(const Entity& entity);
[[nodiscard]] Transform& GetTransform(Entity& entity);

[[nodiscard]] Transform GetTransform(const Camera& camera);

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
V2_float GetPosition(const T& entity) {
	return GetTransform(entity).GetPosition();
}

template <EntityBase T>
V2_float GetAbsolutePosition(const T& entity) {
	return GetAbsoluteTransform(entity).GetPosition();
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
V2_float GetScale(const T& entity) {
	return GetTransform(entity).GetScale();
}

template <EntityBase T>
V2_float GetAbsoluteScale(const T& entity) {
	return GetAbsoluteTransform(entity).GetScale();
}

template <EntityBase T>
T& SetPosition(T& entity, const V2_float& position) {
	auto transform{ GetTransform(entity) };
	transform.SetPosition(position);
	return SetTransform(entity, transform);
}

template <EntityBase T>
T& SetPositionX(T& entity, float position_x) {
	return SetPosition(entity, V2_float{ position_x, GetPosition(entity).y });
}

template <EntityBase T>
T& SetPositionY(T& entity, float position_y) {
	return SetPosition(entity, V2_float{ GetPosition(entity).x, position_y });
}

template <EntityBase T>
T& Translate(T& entity, const V2_float& position_difference) {
	return SetPosition(entity, GetPosition(entity) + position_difference);
}

template <EntityBase T>
T& TranslateX(T& entity, float position_x_difference) {
	return Translate(entity, V2_float{ position_x_difference, 0.0f });
}

template <EntityBase T>
T& TranslateY(T& entity, float position_y_difference) {
	return Translate(entity, V2_float{ 0.0f, position_y_difference });
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
	auto transform{ GetTransform(entity) };
	transform.SetRotation(rotation);
	return SetTransform(entity, transform);
}

template <EntityBase T>
T& Rotate(T& entity, float angle_difference) {
	return SetRotation(entity, GetRotation(entity) + angle_difference);
}

template <EntityBase T>
T& SetScale(T& entity, const V2_float& scale) {
	auto transform{ GetTransform(entity) };
	transform.SetScale(scale);
	return SetTransform(entity, transform);
}

template <EntityBase T>
T& SetScale(T& entity, float scale) {
	return SetScale(entity, V2_float{ scale });
}

template <EntityBase T>
T& SetScaleX(T& entity, float scale_x) {
	return SetScale(entity, V2_float{ scale_x, GetScale(entity).y });
}

template <EntityBase T>
T& SetScaleY(T& entity, float scale_y) {
	return SetScale(entity, V2_float{ GetScale(entity).x, scale_y });
}

template <EntityBase T>
T& Scale(T& entity, const V2_float& scale_multiplier) {
	return SetScale(entity, GetScale(entity) * scale_multiplier);
}

template <EntityBase T>
T& ScaleX(T& entity, float scale_x_multiplier) {
	V2_float scale{ GetScale(entity) };
	scale.x *= scale_x_multiplier;
	return SetScale(entity, scale);
}

template <EntityBase T>
T& ScaleY(T& entity, float scale_y_multiplier) {
	V2_float scale{ GetScale(entity) };
	scale.y *= scale_y_multiplier;
	return SetScale(entity, scale);
}

} // namespace ptgn