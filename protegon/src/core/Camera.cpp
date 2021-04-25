#include "Camera.h"

#include "core/Engine.h"

#include "ecs/components/SpriteComponent.h"
#include "ecs/components/CollisionComponent.h"
#include "ecs/components/TransformComponent.h"

namespace engine {

void Camera::ClampToBound() {
	scale = Clamp(scale, scale - scale_bound, scale + scale_bound);
}

void Camera::CenterOn(const V2_double& point, const V2_double& size, std::size_t display_index) {
	auto display_size{ Engine::GetDisplay(display_index).first.GetSize() };
	offset = point + size / 2.0 - display_size / 2.0 / scale;
}

} // namespace engine