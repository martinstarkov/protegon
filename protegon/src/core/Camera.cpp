#include "Camera.h"

#include "core/Window.h"

namespace engine {

Camera::Camera(const V2_double& scale,
			   V2_double zoom_speed,
			   V2_double scale_bound) :
	scale{ scale },
	zoom_speed{ zoom_speed },
	scale_bound{ scale_bound } 
{}

void Camera::ClampToBound() {
	scale = math::Clamp(scale, scale - scale_bound, scale + scale_bound);
}

void Camera::CenterOn(const V2_double& point, const V2_double& size) {
	position = point + size / 2.0 - (Window::GetSize() / 2.0) / scale;
}

} // namespace engine