#include "Camera.h"

#include "core/Engine.h"

#include "ecs/components/SpriteComponent.h"
#include "ecs/components/CollisionComponent.h"
#include "ecs/components/TransformComponent.h"

namespace engine {

void Camera::LimitScale(V2_double max_scale) {
	if (scale.x > 1.0f + max_scale.x) {
		scale.x = 1.0f + max_scale.x;
	}
	if (scale.y > 1.0f + max_scale.y) {
		scale.y = 1.0f + max_scale.y;
	}
	if (scale.x < 1.0f - max_scale.x) {
		scale.x = 1.0f - max_scale.x;
	}
	if (scale.y < 1.0f - max_scale.y) {
		scale.y = 1.0f - max_scale.y;
	}
}

void Camera::Center(V2_double point, V2_double size) {
	offset = -point - size / 2.0 + engine::Engine::ScreenSize() / 2.0 / scale;
}

} // namespace engine