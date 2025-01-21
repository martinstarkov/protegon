#pragma once

#include <iosfwd>

#include "core/manager.h"
#include "math/geometry/polygon.h"
#include "math/matrix4.h"
#include "math/quaternion.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "renderer/flip.h"
#include "renderer/origin.h"

namespace ptgn {

struct WindowResizedEvent;

namespace impl {

class Game;
class CameraManager;

} // namespace impl

class Camera {
public:
	// Default constructed camera will be continuously window sized.
	Camera();
	Camera(const Camera& copy);
	Camera& operator=(const Camera& copy) noexcept;
	Camera(Camera&& move) noexcept;
	Camera& operator=(Camera&& move) noexcept;
	~Camera();

	// If continuously is true, camera will subscribe to window resize event.
	// Set the camera to be the size of the window and centered on the window.
	void SetToWindow(bool continuously = true);

	// If continuously is true, camera will subscribe to window resize event.
	// Set the camera to be centered on the window.
	void CenterOnWindow(bool continuously = false);

	// If continuously is true, camera will subscribe to window resize event.
	// Set the camera to be the size of the window.
	void SetSizeToWindow(bool continuously = false);

	// Set the camera to be centered on area of the given size. Effectively the same as changing the
	// size and position of the camera.
	void CenterOnArea(const V2_float& size);

	// Transforms a window relative pixel coordinate to being relative to this camera.
	// @param screen_coordinate The coordinate to be transformed.
	[[nodiscard]] V2_float ScreenToCamera(const V2_float& screen_coordinate) const;

	// Origin at the top left.
	[[nodiscard]] Rect GetRect() const;

	[[nodiscard]] V2_float GetSize() const;

	[[nodiscard]] float GetZoom() const;

	// Use Min() and Max() of rectangle to find top left and bottom right bounds of camera.
	[[nodiscard]] Rect GetBounds() const;

	// @param origin What point on the camera the position represents.
	// @return The position of the camera.
	[[nodiscard]] V2_float GetPosition(Origin origin = Origin::Center) const;

	// (yaw, pitch, roll) (radians).
	[[nodiscard]] V3_float GetOrientation() const;

	// Orientation as a quaternion.
	[[nodiscard]] Quaternion GetQuaternion() const;

	[[nodiscard]] Flip GetFlip() const;

	void SetFlip(Flip flip);

	// Camera bounds only apply along aligned axes. In other words: rotated cameras can see outside
	// the bounding box.
	void SetBounds(const Rect& bounding_box);

	void SetSize(const V2_float& size);

	// Set point which is at the center of the camera view.
	void SetPosition(const V2_float& new_position);

	void Translate(const V2_float& position_change);

	void SetZoom(float new_zoom);

	void Zoom(float zoom_change_amount);

	// (yaw, pitch, roll) in radians.
	void SetRotation(const V3_float& new_angle_radians);

	// (yaw, pitch, roll) in radians.
	void Rotate(const V3_float& angle_change_radians);

	// Yaw in radians.
	void SetRotation(float yaw_radians);

	// Yaw in radians.
	void Rotate(float yaw_change_radians);

	// Angle in radians.
	void SetYaw(float angle_radians);

	// Angle in radians.
	void Yaw(float angle_change_radians);

	// Angle in radians.
	void SetPitch(float angle_radians);

	// Angle in radians.
	void Pitch(float angle_change_radians);

	// Angle in radians.
	void SetRoll(float angle_radians);

	// Angle in radians.
	void Roll(float angle_change_radians);

	void PrintInfo() const;

	[[nodiscard]] const Matrix4& GetViewProjection() const;

	operator Matrix4() const;

protected:
	[[nodiscard]] const Matrix4& GetView() const;
	[[nodiscard]] const Matrix4& GetProjection() const;

	void RefreshBounds();

	void SetPosition(const V3_float& new_position);

	void SubscribeToEvents();
	void OnWindowResize(const WindowResizedEvent& e);

	void RecalculateView() const;
	void RecalculateProjection() const;
	void RecalculateViewProjection() const;

	void Reset();

	// Top left position of camera.
	V3_float position;

	V2_float size;

	float zoom{ 1.0f };

	V3_float orientation;

	// If rectangle IsZero(), no position bounds are enforced.
	Rect bounding_box;

	Flip flip{ Flip::None };

	// Mutable used because view projection is recalculated only upon retrieval to reduce matrix
	// multiplications.
	mutable Matrix4 view{ 1.0f };
	mutable Matrix4 projection{ 1.0f };
	mutable Matrix4 view_projection{ 1.0f };
	mutable bool recalculate_view{ false };
	mutable bool recalculate_projection{ false };

	bool center_to_window{ true };
	bool resize_to_window{ true };

private:
	friend class impl::CameraManager;

	struct UninitializedCamera {};

	explicit Camera(const UninitializedCamera&) {}
};

inline std::ostream& operator<<(std::ostream& os, const ptgn::Camera& c) {
	os << "[center position: " << c.GetPosition() << ", size: " << c.GetSize() << "]";
	return os;
}

namespace impl {

class CameraManager : public MapManager<Camera> {
public:
	CameraManager()									   = default;
	~CameraManager() override						   = default;
	CameraManager(CameraManager&&) noexcept			   = default;
	CameraManager& operator=(CameraManager&&) noexcept = default;
	CameraManager(const CameraManager&)				   = delete;
	CameraManager& operator=(const CameraManager&)	   = delete;

	template <typename TKey>
	void SetPrimary(const TKey& key) {
		SetPrimaryImpl(GetInternalKey(key));
	}

	void SetPrimary(const Camera& camera);

	[[nodiscard]] const Camera& GetPrimary() const;

	void Reset();

private:
	friend class Game;

	void Init();

	// Informs the renderer's current target that the primary camera has changed.
	void CameraChanged() const;

	void SetPrimaryImpl(const InternalKey& key);

	Camera primary_camera_{ Camera::UninitializedCamera{} };
};

} // namespace impl

/*
class CameraController;

struct PerspectiveCamera {
	PerspectiveCamera(
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
	Matrix4 GetViewMatrix() const {
		return Matrix4::LookAt(position, position + front, up);
	}

	Matrix4 GetProjectionMatrix() const {
		return projection;
	}

	void SetProjectionMatrix(const Matrix4& p) {
		projection = p;
	}

	Matrix4 projection{ 1.0f };

	// Set later:

	V3_float right;

private:
	friend class CameraController;

	// calculates the front vector from the Camera's (updated) Euler Angles
	void UpdateVectors() {
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
	// PerspectiveCamera camera;

	// camera options
	float speed{ DEFAULT_SPEED };
	float sensitivity{ DEFAULT_SENSITIVITY };
	float zoom{ DEFAULT_ZOOM };

	CameraController() = delete;

	// constructor with vectors
	CameraController(
			const V3_float& position, const V3_float& up = { 0.0f, 1.0f, 0.0f },
			const V3_float& angle = DEFAULT_ANGLE
	) :
		camera{ position, { 0.0f, 0.0f, -1.0f }, up, up, angle } {
		camera.UpdateVectors();
		// Optional:
		SubscribeToMouseEvents();
	}

	~CameraController() {
		UnsubscribeFromMouseEvents();
	}

	// processes input received from any keyboard-like input system. Accepts input parameter in
the
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

	// processes input received from a mouse input system. Expects the offset value in both the
x
	// and y direction.
	void Rotate(float xoffset, float yoffset, float zoffset = 0.0f, bool constrain_pitch = true)
{ xoffset *= sensitivity; yoffset *= sensitivity; zoffset *= sensitivity;

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

	// processes input received from a mouse scroll-wheel event. Only requires input on the
vertical
	// wheel-axis
	void Zoom(float yoffset) {
		zoom -= (float)yoffset;
		zoom  = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
	}

	void SubscribeToMouseEvents();
	void UnsubscribeFromMouseEvents();
	void OnMouseMoveEvent(const MouseMoveEvent& e);
};
*/

// Transforms a window relative pixel coordinate to being relative to the specified viewport and
// primary game camera.
// @param viewport The viewport relative to which the screen coordinate is transformed.
// @param camera The camera relative to which the screen coordinate is transformed.
// @param screen_coordinate The coordinate to be transformed.
[[nodiscard]] V2_float ScreenToViewport(
	const Rect& viewport, const Camera& camera, const V2_float& screen_coordinate
);

} // namespace ptgn
