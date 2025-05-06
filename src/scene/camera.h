#pragma once

#include <array>
#include <iosfwd>

#include "components/generic.h"
#include "core/entity.h"
#include "core/game_object.h"
#include "core/manager.h"
#include "math/matrix4.h"
#include "math/quaternion.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "rendering/api/color.h"
#include "rendering/api/flip.h"
#include "rendering/api/origin.h"
#include "core/time.h"
#include "core/tween.h"
#include "rendering/graphics/vfx/tween_effects.h"

namespace ptgn {

struct WindowResizedEvent;
class Scene;
class CameraManager;

namespace impl {

// TODO: Add all these as serializable components.

struct CameraPanStart : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct CameraZoomStart : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;
};

struct CameraRotationStart : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;
};

struct CameraLerp : public Vector2Component<float> {
	using Vector2Component::Vector2Component;

	CameraLerp() : Vector2Component{ V2_float{ 1.0f, 1.0f } } {}
};

struct CameraDeadzone : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct CameraOffset : public Vector2Component<float> {
	using Vector2Component::Vector2Component;
};

struct CameraInfo {
	CameraInfo();
	CameraInfo(const CameraInfo& other);
	CameraInfo& operator=(const CameraInfo& other);
	CameraInfo(CameraInfo&& other) noexcept;
	CameraInfo& operator=(CameraInfo&& other) noexcept;
	~CameraInfo();

	[[nodiscard]] float GetZoom() const;
	[[nodiscard]] V2_float GetSize() const;
	[[nodiscard]] V2_float GetPosition() const;
	// Gets only the yaw.
	[[nodiscard]] float GetRotation() const;

	void SetZoom(float new_zoom);

	void SetSize(const V2_float& new_size);

	// Sets yaw only.
	void SetRotation(float yaw_angle_radians);

	void SetRotation(const V3_float& new_angle_radians);

	void SetPosition(const V2_float& new_position);
	void SetPosition(const V3_float& new_position);

	void SetBounds(const V2_float& position, const V2_float& size);

	// Will resize the camera.
	void SubscribeToEvents() noexcept;

	void UnsubscribeFromEvents() noexcept;

	void OnWindowResize(const WindowResizedEvent& e) noexcept;

	void RefreshBounds() noexcept;

	// If bounding_box_size.IsZero(), returns position.
	[[nodiscard]] static V2_float ClampToBounds(
		V2_float position, const V2_float& bounding_box_position, const V2_float& bounding_box_size,
		const V2_float& camera_size, float camera_zoom
	);

	// TODO: Convert some of this to be components.
	struct Data {
		V2_float viewport_position;
		V2_float viewport_size;

		// Top left position of camera.
		V3_float position;

		V2_float size;

		float zoom{ 1.0f };

		// If true, rounds camera position to pixel precision.
		// TODO: Check that this works.
		bool pixel_rounding{ false };

		V3_float orientation;

		// Top left position.
		V2_float bounding_box_position;
		// If size is {}, no bounds are enforced.
		V2_float bounding_box_size;

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
	};

	Data data;
};

} // namespace impl

class Camera : public GameObject {
public:
	Camera() = default;
	explicit Camera(Manager& manager);
	Camera(const Camera&)				 = delete;
	Camera& operator=(const Camera&)	 = delete;
	Camera(Camera&&) noexcept			 = default;
	Camera& operator=(Camera&&) noexcept = default;
	~Camera()							 = default;

	void SetPixelRounding(bool enabled);
	[[nodiscard]] bool IsPixelRoundingEnabled() const;

	// @param target_position Position to pan to.
	// @param duration Duration of pan.
	// @param ease Easing function for pan.
	// @param force If false, the pan is queued in the pan queue, if true the pan is executed
	// immediately, clearing any previously queued pans or target following.
	Tween& PanTo(
		const V2_float& target_position, milliseconds duration, TweenEase ease = TweenEase::Linear,
		bool force = false
	);

	// @param target_zoom Zoom level to go to.
	// @param duration Duration of zoom.
	// @param ease Easing function for zoom.
	// @param force If false, the zoom is queued in the zoom queue, if true the zoom is executed
	// immediately, clearing any previously queued zooms.
	Tween& ZoomTo(
		float target_zoom, milliseconds duration, TweenEase ease = TweenEase::Linear,
		bool force = false
	);

	// @param target_angle Angle (in radians) to rotate to (refer to diagram below). If positive,
	// rotation is clockwise, if negative rotation is counter-clockwise.
	// @param duration Duration of rotation.
	// @param ease Easing function for rotation.
	// @param force If false, the rotation is queued in the rotation queue, if true the rotation is
	// executed immediately, clearing any previously queued rotations.
	/* Range: (-3.14159, 3.14159].
	 * (clockwise positive).
	 *            -1.5708
	 *               |
	 *    3.14159 ---o--- 0
	 *               |
	 *             1.5708
	 */
	Tween& RotateTo(
		float target_angle, milliseconds duration, TweenEase ease = TweenEase::Linear,
		bool force = false
	);

	// Execute a continuous shake effect on the camera.
	// @param intensity Range: [0, 1] for how intensely the camera shakes, 1 being the most intense.
	// @param duration Duration of the shake effect.
	// @param config Shake specification (max translation, etc).
	// @param force If false, the shake is queued in the shake queue, if true the shake is executed
	// immediately, clearing any previously queued shake effects.
	Tween& Shake(
		float intensity, milliseconds duration, const ShakeConfig& config = {}, bool force = false
	);

	void StopShake(bool force = true);

	// Execute a momentary shake effect on the camera.
	// @param intensity Range: [0, 1] for how intensely the camera shakes, 1 being the most intense.
	// @param config Shake specification (max translation, etc).
	// @param force If false, the shake is queued in the shake queue, if true the shake is executed
	// immediately, clearing any previously queued shake effects.
	Tween& Shake(float intensity, const ShakeConfig& config = {}, bool force = false);

	// Note: If the target entity is destroyed, set to null, or its transform component is removed
	// the camera will stop following it.
	// @param target The target entity for the camera to follow.
	// @param force If false, the follow is queued in the pan queue, if true the follow is executed
	// immediately, clearing any previously queued pans or target following.
	void StartFollow(Entity target, bool force = false);

	// Stop following the current target and moves onto to the next item in the pan queue.
	// @param force If true, clears the pan queue.
	void StopFollow(bool force = false);

	// @param color Starting color.
	// @param duration Duration of fade.
	// @param ease Easing function for the fade.
	// @param force If false, the fade is queued in the fade queue, if true the fade is executed
	// immediately, clearing any previously queued fades.
	Tween& FadeFrom(
		const Color& color, milliseconds duration, TweenEase ease = TweenEase::Linear,
		bool force = false
	);

	// @param color End color.
	// @param duration Duration of fade.
	// @param ease Easing function for the fade.
	// @param force If false, the fade is queued in the fade queue, if true the fade is executed
	// immediately, clearing any previously queued fades.
	Tween& FadeTo(
		const Color& color, milliseconds duration, TweenEase ease = TweenEase::Linear,
		bool force = false
	);

	Tween& SetColor(const Color& color, bool force = false);

	// Top left position.
	[[nodiscard]] V2_float GetViewportPosition() const;
	[[nodiscard]] V2_float GetViewportSize() const;

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

	// Transforms a window relative pixel coordinate to being relative to the camera.
	// @param screen_relative_coordinate The coordinate to be transformed.
	[[nodiscard]] V2_float TransformToCamera(const V2_float& screen_relative_coordinate) const;

	// Transforms a camera relative pixel coordinate to being relative to the screen.
	// @param camera_relative_coordinate The coordinate to be transformed.
	[[nodiscard]] V2_float TransformToScreen(const V2_float& camera_relative_coordinate) const;

	[[nodiscard]] std::array<V2_float, 4> GetVertices() const;

	[[nodiscard]] V2_float GetSize() const;

	[[nodiscard]] float GetZoom() const;

	[[nodiscard]] V2_float GetBoundsPosition() const;
	[[nodiscard]] V2_float GetBoundsSize() const;

	// @param origin What point on the camera the position represents.
	// @return The position of the camera.
	[[nodiscard]] V2_float GetPosition(Origin origin = Origin::Center) const;

	// @return (yaw, pitch, roll) (radians).
	[[nodiscard]] V3_float GetOrientation() const;

	// @return Yaw (2D rotation) (radians).
	[[nodiscard]] float GetRotation() const;

	// Orientation as a quaternion.
	[[nodiscard]] Quaternion GetQuaternion() const;

	[[nodiscard]] Flip GetFlip() const;

	void SetFlip(Flip flip);

	// Camera bounds only apply along aligned axes. In other words: rotated cameras can see outside
	// the bounding box.
	// @param position Top left position of the bounds.
	void SetBounds(const V2_float& position, const V2_float& size);

	void SetSize(const V2_float& size);

	// Set the point which is at the center of the camera view.
	void SetPosition(const V2_float& new_position);

	void Translate(const V2_float& position_change);

	void SetZoom(float new_zoom);

	void Zoom(float zoom_change_amount);

	// (yaw, pitch, roll) in radians.
	void SetRotation(const V3_float& new_angle_radians);

	// (yaw, pitch, roll) in radians.
	void Rotate(const V3_float& angle_change_radians);

	// Set 2D rotation angle in radians.
	/* Range: (-3.14159, 3.14159].
	 * (clockwise positive).
	 *            -1.5708
	 *               |
	 *    3.14159 ---o--- 0
	 *               |
	 *             1.5708
	 */
	void SetRotation(float angle_radians);

	// Rotate camera in 2D (radians).
	/* Range: (-3.14159, 3.14159].
	 * (clockwise positive).
	 *            -1.5708
	 *               |
	 *    3.14159 ---o--- 0
	 *               |
	 *             1.5708
	 */
	void Rotate(float angle_change_radians);

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

	// Only applies when camera is following a target.
	// Range: [0, 1]. Determines how smoothly the camera tracks to the target's position. 1 for
	// instant tracking, 0 to disable tracking.
	void SetLerp(const V2_float& lerp = V2_float{ 1.0f });

	[[nodiscard]] V2_float GetLerp() const;

	// Only applies when camera is following a target.
	// Deadzone is a rectangle centered on the target inside of which the camera does not track the
	// target. If {}, deadzone is removed.
	void SetDeadzone(const V2_float& size = {});

	[[nodiscard]] V2_float GetDeadzone() const;

	// Only applies when camera is following a target.
	// Sets an offset such that the camera follows target.transform + offset.
	// If {}, offset is removed.
	void SetFollowOffset(const V2_float& offset = {});

	[[nodiscard]] V2_float GetFollowOffset() const;

	void PrintInfo() const;

	[[nodiscard]] const Matrix4& GetViewProjection() const;

	operator Matrix4() const;

protected:
	friend class CameraManager;

	// @param start_color Starting color.
	// @param end_color Ending color.
	// @param duration Duration of fade.
	// @param ease Easing function for the fade.
	// @param force If false, the fade is queued in the fade queue, if true the fade is executed
	// immediately, clearing any previously queued fades.
	Tween& FadeFromTo(
		const Color& start_color, const Color& end_color, milliseconds duration, TweenEase ease,
		bool force
	);

	[[nodiscard]] const Matrix4& GetView() const;
	[[nodiscard]] const Matrix4& GetProjection() const;

	// Set the point which is at the center of the camera view.
	void SetPosition(const V3_float& new_position);

	void RecalculateView(const Transform& offset_transform) const;
	void RecalculateProjection() const;
	void RecalculateViewProjection() const;

	GameObject pan_effects_;
	GameObject rotation_effects_;
	GameObject zoom_effects_;
	GameObject fade_effects_;
};

inline std::ostream& operator<<(std::ostream& os, const ptgn::Camera& c) {
	os << "[center position: " << c.GetPosition() << ", size: " << c.GetSize() << "]";
	return os;
}

class CameraManager {
public:
	// Reset primary camera back to window and reset window camera in case it has been
	// modified.
	void Reset();

	Camera primary;
	Camera window;

private:
	friend class Scene;

	void Init(Manager& manager);
};

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

} // namespace ptgn