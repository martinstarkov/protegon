#pragma once

#include <cstdint>

#include "components/generic.h"
#include "math/vector2.h"
#include "serialization/serializable.h"
#include "utility/flags.h"

namespace ptgn {

class Entity;
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

	friend bool operator!=(const Transform& a, const Transform& b) {
		return !operator==(a, b);
	}

	[[nodiscard]] V2_float GetPosition() const;
	[[nodiscard]] float GetRotation() const;
	[[nodiscard]] V2_float GetScale() const;

	Transform& SetPosition(const V2_float& position);
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

private:
	friend class Scene;

	V2_float position_;
	float rotation_{ 0.0f };
	V2_float scale_{ 1.0f, 1.0f };

	// By default all flags are dirty.
	Flags<impl::TransformDirty> dirty_flags_{ impl::TransformDirty::Position |
											  impl::TransformDirty::Rotation |
											  impl::TransformDirty::Scale };

	PTGN_SERIALIZER_REGISTER_NAMED_IGNORE_DEFAULTS(
		Transform, KeyValue("position", position_), KeyValue("rotation", rotation_),
		KeyValue("scale", scale_), KeyValue("dirty_flags", dirty_flags_)
	)
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
[[nodiscard]] Transform GetAbsoluteTransform(
	const Entity& entity, bool relative_to_scene_primary_camera = true
);

// @return The transform of the entity used for drawing it with respect to its parent scene
// camera transform. This includes any shake of bounce offsets which are only used for
// rendering.
[[nodiscard]] Transform GetDrawTransform(const Entity& entity);

// Set the relative position of the entity with respect to its parent entity, camera, or
// scene camera position.
// @return *this.
Entity& SetPosition(Entity& entity, const V2_float& position);
Entity& SetPositionX(Entity& entity, float position_x);
Entity& SetPositionY(Entity& entity, float position_y);

// position += position_difference
Entity& Translate(Entity& entity, const V2_float& position_difference);
Entity& TranslateX(Entity& entity, float position_x_difference);
Entity& TranslateY(Entity& entity, float position_y_difference);

// @return The relative position of the entity with respect to its parent entity, camera, or
// scene camera position.
[[nodiscard]] V2_float GetPosition(const Entity& entity);

// @return The absolute position of the entity with respect to its parent scene camera position.
[[nodiscard]] V2_float GetAbsolutePosition(const Entity& entity);

// Set 2D rotation angle in radians.
/* Range: (-3.14159, 3.14159].
 * (clockwise positive).
 *            -1.5708
 *               |
 *    3.14159 ---o--- 0
 *               |
 *             1.5708
 */
// Set the relative rotation of the entity with respect to its parent entity, camera, or
// scene camera rotation. Clockwise positive. Unit: Radians.
// @return *this.
Entity& SetRotation(Entity& entity, float rotation);

// rotation += angle_difference
Entity& Rotate(Entity& entity, float angle_difference);

// @return The relative rotation of the entity with respect to its parent entity, camera, or
// scene camera rotation. Clockwise positive. Unit: Radians.
[[nodiscard]] float GetRotation(const Entity& entity);

// @return The absolute rotation of the entity with respect to its parent scene camera rotation.
// Clockwise positive. Unit: Radians.
[[nodiscard]] float GetAbsoluteRotation(const Entity& entity);

// Set the relative scale of the entity with respect to its parent entity, camera, or
// scene camera scale.
// @return *this.
Entity& SetScale(Entity& entity, const V2_float& scale);
Entity& SetScale(Entity& entity, float scale);
Entity& SetScaleX(Entity& entity, float scale_x);
Entity& SetScaleY(Entity& entity, float scale_y);

// scale *= scale_multiplier
Entity& Scale(Entity& entity, const V2_float& scale_multiplier);
Entity& ScaleX(Entity& entity, float scale_x_multiplier);
Entity& ScaleY(Entity& entity, float scale_y_multiplier);

// @return The relative scale of the entity with respect to its parent entity, camera, or
// scene camera scale.
[[nodiscard]] V2_float GetScale(const Entity& entity);

// @return The absolute scale of the entity with respect to its parent scene camera scale.
[[nodiscard]] V2_float GetAbsoluteScale(const Entity& entity);

} // namespace ptgn