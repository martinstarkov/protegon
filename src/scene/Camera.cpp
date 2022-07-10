#include "Camera.h"

#include <cassert> // assert

#include "core/Window.h"
#include "components/Transform.h"
#include "physics/Shape.h"

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

void Camera::ZoomIn(const V2_double& amount) {
	scale += amount;
	ClampToBound();
}

void Camera::ZoomIn() {
	scale += zoom_speed;
	ClampToBound();
}

void Camera::ZoomOut(const V2_double& amount) {
	scale -= amount;
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

void Camera::CenterOn(const ecs::Entity& entity, bool use_size) {
	//assert(entity.HasComponent<Transform>() &&
	//	   "Cannot center camera on entity without TransformComponent");
	//auto& transform{ entity.GetComponent<Transform>() };
	//V2_int size;
	//if (use_size) {
	//	assert(entity.HasComponent<physics::Shape>() &&
	//		   "Cannot center camera on entity size without ShapeComponent");
	//	auto& shape{ entity.GetComponent<physics::Shape>() };
	//	auto type{ shape.GetType() };
	//	// Only define size for AABB as circle positions are relative to center already.
	//	if (type == physics::ShapeType::AABB) {
	//		size = shape.GetSize();
	//	}
	//}
	//CenterOn(transform.position, size);
}

} // namespace ptgn