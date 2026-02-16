#pragma once

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

namespace impl {

struct IgnoreParentTransform {};

} // namespace impl

// Set the transform of the entity with respect to its parent entity.
// @return *this.
Entity SetTransform(Entity entity, Transform transform);

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