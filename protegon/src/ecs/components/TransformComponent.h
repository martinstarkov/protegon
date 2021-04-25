#pragma once

#include "physics/Transform.h"

namespace engine {

struct TransformComponent {
	TransformComponent() = default;
	TransformComponent(const Transform& transform) : transform{ transform } {}
	Transform transform;
};

} // namespace engine