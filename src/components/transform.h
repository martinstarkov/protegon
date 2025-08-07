#pragma once

#include "components/generic.h"
#include "math/vector2.h"
#include "serialization/serializable.h"

namespace ptgn {

class Entity;

struct Transform {
	Transform() = default;

	explicit Transform(const V2_float& position);

	Transform(const V2_float& position, float rotation, const V2_float& scale = { 1.0f, 1.0f });

	[[nodiscard]] Transform RelativeTo(const Transform& parent) const;

	[[nodiscard]] Transform InverseRelativeTo(const Transform& parent) const;

	[[nodiscard]] float GetAverageScale() const;

	friend bool operator==(const Transform& a, const Transform& b) {
		return a.position == b.position && a.rotation == b.rotation && a.scale == b.scale;
	}

	friend bool operator!=(const Transform& a, const Transform& b) {
		return !(a == b);
	}

	V2_float position;
	float rotation{ 0.0f };
	V2_float scale{ 1.0f, 1.0f };

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(Transform, position, rotation, scale)
};

// Set the relative transform of the entity with respect to its parent entity, camera, or
// scene camera transform.
// @return *this.
Entity& SetTransform(Entity& entity, const Transform& transform);

// @return The relative transform of the entity with respect to its parent entity, camera, or
// scene camera transform.
[[nodiscard]] Transform GetTransform(const Entity& entity);
[[nodiscard]] Transform& GetTransform(Entity& entity);

// @return The absolute transform of the entity with respect to its parent scene camera
// transform.
[[nodiscard]] Transform GetAbsoluteTransform(const Entity& entity);

// @return The transform of the entity used for drawing it with respect to its parent scene
// camera transform. This includes any shake of bounce offsets which are only used for
// rendering.
[[nodiscard]] Transform GetDrawTransform(const Entity& entity);

// Set the relative position of the entity with respect to its parent entity, camera, or
// scene camera position.
// @return *this.
Entity& SetPosition(Entity& entity, const V2_float& position);

// @return The relative position of the entity with respect to its parent entity, camera, or
// scene camera position.
[[nodiscard]] V2_float GetPosition(const Entity& entity);
[[nodiscard]] V2_float& GetPosition(Entity& entity);

// @return The absolute position of the entity with respect to its parent scene camera position.
[[nodiscard]] V2_float GetAbsolutePosition(const Entity& entity);

// Set the relative rotation of the entity with respect to its parent entity, camera, or
// scene camera rotation. Clockwise positive. Unit: Radians.
// @return *this.
Entity& SetRotation(Entity& entity, float rotation);

// @return The relative rotation of the entity with respect to its parent entity, camera, or
// scene camera rotation. Clockwise positive. Unit: Radians.
[[nodiscard]] float GetRotation(const Entity& entity);
[[nodiscard]] float& GetRotation(Entity& entity);

// @return The absolute rotation of the entity with respect to its parent scene camera rotation.
// Clockwise positive. Unit: Radians.
[[nodiscard]] float GetAbsoluteRotation(const Entity& entity);

// Set the relative scale of the entity with respect to its parent entity, camera, or
// scene camera scale.
// @return *this.
Entity& SetScale(Entity& entity, const V2_float& scale);
Entity& SetScale(Entity& entity, float scale);

// @return The relative scale of the entity with respect to its parent entity, camera, or
// scene camera scale.
[[nodiscard]] V2_float GetScale(const Entity& entity);
[[nodiscard]] V2_float& GetScale(Entity& entity);

// @return The absolute scale of the entity with respect to its parent scene camera scale.
[[nodiscard]] V2_float GetAbsoluteScale(const Entity& entity);

namespace impl {

struct IgnoreParentTransform : public ArithmeticComponent<bool> {
	using ArithmeticComponent::ArithmeticComponent;

	IgnoreParentTransform() : ArithmeticComponent{ true } {}

	PTGN_SERIALIZER_REGISTER_NAMELESS_IGNORE_DEFAULTS(IgnoreParentTransform, value_)
};

} // namespace impl

} // namespace ptgn