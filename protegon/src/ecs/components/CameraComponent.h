#pragma once

#include <cstdlib> // std::size_t

#include "core/Camera.h"

namespace engine {

struct CameraComponent {
	CameraComponent() = default;
	CameraComponent(bool primary, 
					std::size_t display_index = 0) : 
		primary{ primary }, 
		display_index{ display_index } {
	}
	CameraComponent(const Camera& camera, 
					bool primary = false, 
					std::size_t display_index = 0) : 
		camera{ camera }, 
		primary{ primary }, 
		display_index{ display_index } {
	}
	Camera camera;
	bool primary{ false };
	std::size_t display_index{ 0 };
};

} // namespace engine