#pragma once

#include "core/Camera.h"

namespace ptgn {

struct CameraComponent {
	CameraComponent() = default;
	CameraComponent(const Camera& camera,
					bool primary = false) : 
		camera{ camera },
		primary{ primary } {}
	Camera camera;
	bool primary{ false };
};

} // namespace ptgn