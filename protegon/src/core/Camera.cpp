#include "Camera.h"

#include "core/Engine.h"

#include "ecs/components/SpriteComponent.h"
#include "ecs/components/CollisionComponent.h"
#include "ecs/components/TransformComponent.h"

namespace engine {

void Camera::ClampScale(const V2_double& max_scale) {
	Clamp(scale, scale - max_scale, scale + max_scale);
}

void Camera::Center(const V2_double& point, const V2_double& size) {
	auto window_size{ Engine::GetDisplay().first.GetSize() };
	offset = point + size / 2.0 - window_size / 2.0 / scale;
}

} // namespace engine