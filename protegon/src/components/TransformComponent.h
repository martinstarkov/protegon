#pragma once

#include "physics/Transform.h"

namespace ptgn {

struct TransformComponent {
	TransformComponent() = default;
	TransformComponent(const Transform& transform) : transform{ transform } {}
	Transform transform;
};

} // namespace ptgn