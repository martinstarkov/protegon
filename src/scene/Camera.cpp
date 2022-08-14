#include "Camera.h"

#include <cassert> // assert

#include "interface/Window.h"

namespace ptgn {

void Camera::ZoomIn(const V2_float& amount) {
	scale += amount;
	ClampZoom();
}

void Camera::ZoomIn() {
	scale += zoom_speed;
	ClampZoom();
}

void Camera::ZoomOut(const V2_float& amount) {
	scale -= amount;
	ClampZoom();
}

void Camera::ZoomOut() {
	scale -= zoom_speed;
	ClampZoom();
}

void Camera::ClampZoom() {
	scale = Clamp(scale, min_scale, max_scale);
}

void Camera::CenterOn(const V2_float& point, const V2_float& size) {
	position = point + size / 2.0 - (window::GetSize() / 2.0) / scale;
}

V2_float Camera::RelativePosition(const V2_float& object_position) {
	return (object_position - position) * scale;
}
V2_float Camera::RelativeSize(const V2_float& object_size) {
	return object_size * scale;
}

} // namespace ptgn