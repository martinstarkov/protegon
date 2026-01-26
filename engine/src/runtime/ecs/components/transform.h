#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "core/util/concepts.h"
#include "core/util/flags.h"
#include "ecs/components/generic.h"
#include "ecs/entity.h"
#include "math/tolerance.h"
#include "math/vector2.h"
#include "serialization/json/serialize.h"

namespace ptgn {

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

	template <Arithmetic T>
	Transform(const Vector2<T>& position) : position_{ position } {}

	Transform(V2_float position, float rotation, V2_float scale = { 1.0f, 1.0f });

	[[nodiscard]] Transform Inverse() const;

	[[nodiscard]] Transform RelativeTo(const Transform& parent) const;

	[[nodiscard]] Transform InverseRelativeTo(const Transform& parent) const;

	// Dirty flags should not be compared.
	friend bool operator==(const Transform& a, const Transform& b) {
		return a.position_ == b.position_ && NearlyEqual(a.rotation_, b.rotation_) &&
			   a.scale_ == b.scale_;
	}

	[[nodiscard]] V2_float GetPosition() const;

	Transform& SetPosition(V2_float position);
	// Set position along a particular axis: x == 0, y == 1.
	Transform& SetPosition(std::size_t index, float position);
	Transform& SetPositionX(float x);
	Transform& SetPositionY(float y);

	// position += position_difference
	Transform& Translate(V2_float position_difference);
	Transform& TranslateX(float position_x_difference);
	Transform& TranslateY(float position_y_difference);

	// @return Unit: Radians, Direction: Clockwise positive.
	[[nodiscard]] float GetRotation() const;

	// @param rotation Unit: Radians, Direction: Clockwise positive.
	Transform& SetRotation(float rotation);

	// @param angle_difference Unit: Radians, Direction: Clockwise positive.
	// rotation += angle_difference
	Transform& Rotate(float angle_difference);

	// Clamps rotation between [0, 2 pi).
	Transform& ClampRotation();

	// @return (scale_x + scale_y) / 2
	[[nodiscard]] float GetAverageScale() const;

	[[nodiscard]] V2_float GetScale() const;

	Transform& SetScale(float scale);
	Transform& SetScale(V2_float scale);
	Transform& SetScaleX(float x);
	Transform& SetScaleY(float y);

	// scale *= scale_multiplier
	Transform& Scale(V2_float scale_multiplier);
	Transform& ScaleX(float scale_x_multiplier);
	Transform& ScaleY(float scale_y_multiplier);

	[[nodiscard]] bool IsDirty() const;
	void ClearDirtyFlags() const;

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

private:
	friend class Scene;

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

	// @param rotation Unit: Radians, Direction: Clockwise positive.
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
Entity SetTransform(Entity entity, const Transform& transform);

// @return The transform of the entity.
Transform GetTransform(Entity entity);

// @return The transform of the entity with respect to its parent entity.
Transform GetWorldTransform(Entity entity);

V2_float GetPosition(Entity entity);
V2_float GetWorldPosition(Entity entity);

float GetRotation(Entity entity);
float GetWorldRotation(Entity entity);

V2_float GetScale(Entity entity);
V2_float GetWorldScale(Entity entity);

Entity SetPosition(Entity entity, V2_float position);

Entity SetPositionX(Entity entity, float position_x);

Entity SetPositionY(Entity entity, float position_y);

Entity Translate(Entity entity, V2_float position_difference);

Entity TranslateX(Entity entity, float position_x_difference);

Entity TranslateY(Entity entity, float position_y_difference);

// Set 2D rotation angle in radians.
/* Range: (-3.14159, 3.14159].
 * (clockwise positive).
 *            -1.5708
 *               |
 *    3.14159 ---o--- 0
 *               |
 *             1.5708
 */

Entity SetRotation(Entity entity, float rotation);

Entity Rotate(Entity entity, float angle_difference);

Entity SetScale(Entity entity, V2_float scale);
Entity SetScale(Entity entity, float scale);
Entity SetScaleX(Entity entity, float scale_x);
Entity SetScaleY(Entity entity, float scale_y);

Entity Scale(Entity entity, V2_float scale_multiplier);
Entity ScaleX(Entity entity, float scale_x_multiplier);
Entity ScaleY(Entity entity, float scale_y_multiplier);

} // namespace ptgn