#pragma once

#include <algorithm>

#include "protegon/event.h"
#include "protegon/events.h"
#include "protegon/matrix4.h"
#include "protegon/vector2.h"
#include "protegon/vector3.h"

namespace ptgn {

// template<typename T, qualifier Q> GLM_FUNC_QUALIFIER T angle(const qua<T, Q>& x) {
//	if (abs(x.w) > cos_one_over_two<T>()) {
//		const T a = asin(sqrt(x.x * x.x + x.y * x.y + x.z * x.z)) * static_cast<T>(2);
//		if (x.w < static_cast<T>(0)) {
//			return pi<T>() * static_cast<T>(2) - a;
//		}
//		return a;
//	}
//
//	return acos(x.w) * static_cast<T>(2);
// }
//
// template <typename T, qualifier Q>
// GLM_FUNC_QUALIFIER vec<3, T, Q> axis(const qua<T, Q>& x) {
//	const T tmp1 = static_cast<T>(1) - x.w * x.w;
//	if (tmp1 <= static_cast<T>(0)) {
//		return vec<3, T, Q>(0, 0, 1);
//	}
//	const T tmp2 = static_cast<T>(1) / sqrt(tmp1);
//	return vec<3, T, Q>(x.x * tmp2, x.y * tmp2, x.z * tmp2);
// }
//
// template <typename T, qualifier Q>
// GLM_FUNC_QUALIFIER qua<T, Q> angleAxis(const T& angle, const vec<3, T, Q>& v) {
//	const T a(angle);
//	const T s = glm::sin(a * static_cast<T>(0.5));
//
//	return qua<T, Q>(glm::cos(a * static_cast<T>(0.5)), v * s);
// }

// class camera {
//	glm::vec3 m_pos;
//	glm::quat m_orient;
//
// public:
//	camera(void)		   = default;
//	camera(const camera &) = default;
//
//	camera(const glm::vec3 &pos) : m_pos(pos) {}
//
//	camera(const glm::vec3 &pos, const glm::quat &orient) : m_pos(pos), m_orient(orient) {}
//
//	camera &operator=(const camera &) = default;
//
//	const glm::vec3 &position(void) const {
//		return m_pos;
//	}
//
//	const glm::quat &orientation(void) const {
//		return m_orient;
//	}
//
//	glm::mat4 view(void) const {
//		return glm::translate(glm::mat4_cast(m_orient), m_pos);
//	}
//
//	void translate(const glm::vec3 &v) {
//		m_pos += v * m_orient;
//	}
//
//	void translate(float x, float y, float z) {
//		m_pos += glm::vec3(x, y, z) * m_orient;
//	}
//
//	void rotate(float angle, const glm::vec3 &axis) {
//		m_orient *= glm::angleAxis(angle, axis * m_orient);
//	}
//
//	void rotate(float angle, float x, float y, float z) {
//		m_orient *= glm::angleAxis(angle, glm::vec3(x, y, z) * m_orient);
//	}
//
//	void yaw(float angle) {
//		rotate(angle, 0.0f, 1.0f, 0.0f);
//	}
//
//	void pitch(float angle) {
//		rotate(angle, 1.0f, 0.0f, 0.0f);
//	}
//
//	void roll(float angle) {
//		rotate(angle, 0.0f, 0.0f, 1.0f);
//	}
// };

class CameraController;

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

	// returns the view matrix calculated using Euler Angles and the LookAt Matrix
	M4_float GetViewMatrix() const {
		return M4_float::LookAt(position, position + front, up);
	}

	M4_float GetProjectionMatrix() const {
		return projection;
	}

	void SetProjectionMatrix(const M4_float& projection) {
		this->projection = projection;
	}

	M4_float projection{ 1.0f };

	// Set later:

	V3_float right;

private:
	friend class CameraController;

	// calculates the front vector from the Camera's (updated) Euler Angles
	void UpdateVectors() {
		V3_float front;
		front.x = std::cos(angle.x) * std::cos(angle.y);
		front.y = std::sin(angle.y);
		front.z = std::sin(angle.x) * std::cos(angle.y);
		front	= front.Normalized();
		right	= front.Cross(world_up).Normalized();
		up		= right.Cross(front).Normalized();
	}
};

enum CameraDirection {
	Forward,
	Backward,
	Left,
	Right,
	Up,
	Down
};

constexpr const V3_float DEFAULT_ANGLE	  = { 90.0f, 0.0f, 0.0f };
constexpr const float DEFAULT_SPEED		  = 2.5f;
constexpr const float DEFAULT_SENSITIVITY = 5.0f;
constexpr const float DEFAULT_ZOOM		  = 45.0f;
constexpr const float MIN_ZOOM			  = 1.0f;
constexpr const float MAX_ZOOM			  = 45.0f;

// An abstract camera class that processes input and calculates the corresponding Euler Angles,
// Vectors, and Matrices for use in OpenGL
class CameraController {
public:
	Camera camera;

	// camera options
	float speed{ DEFAULT_SPEED };
	float sensitivity{ DEFAULT_SENSITIVITY };
	float zoom{ DEFAULT_ZOOM };

	CameraController() = delete;

	// constructor with vectors
	CameraController(
		const V3_float& position, const V3_float& up = { 0.0f, 1.0f, 0.0f },
		const V3_float& angle = DEFAULT_ANGLE
	) : camera{
		position, { 0.0f, 0.0f, -1.0f }, up, up, angle } {
		camera.UpdateVectors();
		// Optional:
		SubscribeToMouseEvents();
	}

	~CameraController() {
		UnsubscribeFromMouseEvents();
	}

	// processes input received from any keyboard-like input system. Accepts input parameter in the
	// form of camera defined ENUM (to abstract it from windowing systems)
	void Move(CameraDirection direction, float dt) {
		float velocity = speed * dt;
		switch (direction) {
			case CameraDirection::Forward:	camera.position += camera.front * velocity; break;
			case CameraDirection::Backward: camera.position -= camera.front * velocity; break;
			case CameraDirection::Left:		camera.position -= camera.right * velocity; break;
			case CameraDirection::Right:	camera.position += camera.right * velocity; break;
			case CameraDirection::Up:		camera.position += camera.up * velocity; break;
			case CameraDirection::Down:		camera.position -= camera.up * velocity; break;
			default:						break;
		}
	}

	// processes input received from a mouse input system. Expects the offset value in both the x
	// and y direction.
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

		// TODO: Consider this.
		// If we don't constrain the yaw to only use values between 0-360
		// we would lose floating precission with very high values, hence
		// the movement would look like big "steps" instead a smooth one!
		// Yaw = std::fmod((Yaw + xoffset), (GLfloat)360.0f);

		camera.UpdateVectors();
	}

	// processes input received from a mouse scroll-wheel event. Only requires input on the vertical
	// wheel-axis
	void Zoom(float yoffset) {
		zoom -= (float)yoffset;
		zoom  = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
	}

	void SubscribeToMouseEvents();
	void UnsubscribeFromMouseEvents();
	void OnMouseMoveEvent(const MouseMoveEvent& e);
};

} // namespace ptgn