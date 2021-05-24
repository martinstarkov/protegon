#include "Camera.h"

#include "core/Window.h"

namespace engine {

void Camera::ClampToBound() {
	scale = math::Clamp(scale, scale - scale_bound, scale + scale_bound);
}

void Camera::CenterOn(const V2_double& point, const V2_double& size, std::size_t display_index) {
	auto display_size{ Window::GetSize() };
	offset = point + size / 2.0 - display_size / 2.0 / scale;
}

} // namespace engine