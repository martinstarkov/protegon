#pragma once

#include "core/Camera.h"

namespace engine {

struct CameraComponent {
	CameraComponent() = default;
	CameraComponent(const Camera& camera,
					bool primary = false) : 
		camera{ camera }, 
		primary{ primary } {}
	Camera camera;
	bool primary{ false };
};

} // namespace engine