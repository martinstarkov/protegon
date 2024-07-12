#pragma once

#include <algorithm>

#include "event.h"
#include "events.h"
#include "vector2.h"
#include "vector3.h"
#include "matrix4.h"

namespace ptgn {

struct Camera {
	Camera(
		const V3_float& position, const V3_float& front, const V3_float& up,
		const V3_float& world_up, const V3_float& angle
	) :
		position{ position }, front{ front }, up{ up }, world_up{ world_up }, angle{ angle } {}
	// camera attributes
	V3_float position;
	V3_float front;
	V3_float up;
	V3_float world_up;
	// yaw, pitch, roll angles
	V3_float angle;

	// Set later:

	V3_float right;
};

enum CameraDirection {
	Forward,
	Backward,
	Left,
	Right
};

constexpr const V3_float DEFAULT_ANGLE	  = { -90.0f, 0.0f, 0.0f };
constexpr const float DEFAULT_SPEED		= 2.5f;
constexpr const float DEFAULT_SENSITIVITY = 0.00001f;
constexpr const float DEFAULT_ZOOM		= 45.0f;
constexpr const float MIN_ZOOM		= 1.0f;
constexpr const float MAX_ZOOM		= 45.0f;

// An abstract camera class that processes input and calculates the corresponding Euler Angles, Vectors, and Matrices for use in OpenGL
class CameraController {
public:
	Camera camera;

	// camera options
	float speed{ DEFAULT_SPEED };
	float sensitivity{ DEFAULT_SENSITIVITY };
	float zoom{ DEFAULT_ZOOM };

	// constructor with vectors
	CameraController(
		const V3_float& position = {}, const V3_float& up = { 0.0f, 1.0f, 0.0f },
		const V3_float& angle = DEFAULT_ANGLE
	) : camera{
		position, { 0.0f, 0.0f, -1.0f }, up, up, angle } {
		UpdateVectors();
		// Optional:
		SubscribeToMouseEvents();
	}

	~CameraController() {
		UnsubscribeFromMouseEvents();
	}

	// returns the view matrix calculated using Euler Angles and the LookAt Matrix
	M4_float GetViewMatrix() {
		return M4_float::LookAt(camera.position, camera.position + camera.front, camera.up);
	}

	// processes input received from any keyboard-like input system. Accepts input parameter in the
	// form of camera defined ENUM (to abstract it from windowing systems)
	void Move(CameraDirection direction, float dt) {
		float velocity = speed * dt;
		switch (direction) {
			case CameraDirection::Forward:
				camera.position += camera.front * velocity;
				break;
			case CameraDirection::Backward:
				camera.position -= camera.front * velocity;
				break;
			case CameraDirection::Left:
				camera.position -= camera.right * velocity;
				break;
			case CameraDirection::Right:
				camera.position += camera.right * velocity;
				break;
			default: break;
		}
	}

	// processes input received from a mouse input system. Expects the offset value in both the x and y direction.
	void Rotate(float xoffset, float yoffset, float zoffset = 0.0f, bool constrain_pitch = true) {
		xoffset *= sensitivity;
		yoffset *= sensitivity;
		zoffset *= sensitivity;

		camera.angle.x += xoffset;
		camera.angle.y += yoffset;
		camera.angle.z += zoffset;

		// make sure that when pitch is out of bounds, screen doesn't get flipped
		if (constrain_pitch) {
			camera.angle.y = std::clamp(camera.angle.y, DegToRad(-89.0f), DegToRad(89.0f));
		}

		// update Front, Right and Up Vectors using the updated Euler angles
		UpdateVectors();
	}

	// processes input received from a mouse scroll-wheel event. Only requires input on the vertical
	// wheel-axis
	void Zoom(float yoffset) {
		zoom -= (float)yoffset;
		zoom  = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
	}

	void SubscribeToMouseEvents();
	void UnsubscribeFromMouseEvents();
	void OnMouseMoveEvent(const Event<MouseEvent>& e);
private:

	// calculates the front vector from the Camera's (updated) Euler Angles
	void UpdateVectors() {
		// calculate the new Front vector
		V3_float front;
		front.x			= std::cos(camera.angle.x) * std::cos(camera.angle.y);
		front.y			= std::sin(camera.angle.y);
		front.z			= std::sin(camera.angle.x) * std::cos(camera.angle.y);
		camera.front	= front.Normalized();
		// also re-calculate the Right and Up vector
		camera.right = camera.front.Cross(camera.world_up).Normalized(); // normalize the vectors, because their length gets closer to 0 the more you look up or
		   // down which results in slower movement.
		camera.up = camera.right.Cross(camera.front).Normalized();
	}
};

} // namespace ptgn