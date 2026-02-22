#pragma once

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

namespace impl {

struct IgnoreParentTransform {};

} // namespace impl

/// @return The transform of the entity.
Transform GetTransform(Entity entity);

/// @return The transform of the entity with respect to its parent entity.
Transform GetWorldTransform(Entity entity);

/// @return The transform of the entity with respect to its parent entity and including any
/// temporary offsets (e.g., shake or bounce).
Transform GetDrawTransform(Entity entity);

V2_float GetPosition(Entity entity);
V2_float GetWorldPosition(Entity entity);

float GetRotation(Entity entity);
float GetWorldRotation(Entity entity);

V2_float GetScale(Entity entity);
V2_float GetWorldScale(Entity entity);

/// Set the transform of the entity with respect to its parent entity.
void SetTransform(Entity entity, Transform transform);

void SetPosition(Entity entity, V2_float position);
void SetPositionX(Entity entity, float position_x);
void SetPositionY(Entity entity, float position_y);

void Translate(Entity entity, V2_float position_difference);
void TranslateX(Entity entity, float position_x_difference);
void TranslateY(Entity entity, float position_y_difference);

/// Set 2D rotation angle in radians.
/// Range: (-3.14159, 3.14159].
/// (clockwise positive).
///            -1.5708
///               |
///    3.14159 ---o--- 0
///               |
///             1.5708
void SetRotation(Entity entity, float rotation);
void Rotate(Entity entity, float angle_difference);

void SetScale(Entity entity, V2_float scale);
void SetScale(Entity entity, float scale);
void SetScaleX(Entity entity, float scale_x);
void SetScaleY(Entity entity, float scale_y);

void Scale(Entity entity, V2_float scale_multiplier);
void ScaleX(Entity entity, float scale_x_multiplier);
void ScaleY(Entity entity, float scale_y_multiplier);

} // namespace ptgn