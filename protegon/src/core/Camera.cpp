#include "Camera.h"

#include "core/Window.h"

namespace ptgn {

Camera::Camera(const V2_double& scale,
			   V2_double zoom_speed,
			   V2_double min_scale,
			   V2_double max_scale) :
	scale{ scale },
	zoom_speed{ zoom_speed },
	min_scale{ min_scale },
	max_scale{ max_scale }
{}

void Camera::ZoomIn() {
	scale += zoom_speed;
	ClampToBound();
}

void Camera::ZoomOut() {
	scale -= zoom_speed;
	ClampToBound();
}

void Camera::ClampToBound() {
	scale = math::Clamp(scale, min_scale, max_scale);
}

void Camera::CenterOn(const V2_double& point, V2_double size) {
	position = point + size / 2.0 - (Window::GetSize() / 2.0) / scale;
}

} // namespace ptgn