#pragma once

#include <array>
#include <ostream>

#include "components/transform.h"
#include "core/entity.h"
#include "core/manager.h"
#include "core/script.h"
#include "core/time.h"
#include "math/easing.h"
#include "math/matrix4.h"
#include "math/quaternion.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "renderer/api/flip.h"
#include "scene/scene_key.h"
#include "serialization/fwd.h"
#include "serialization/serializable.h"
#include "tweens/follow_config.h"
#include "tweens/shake_config.h"

namespace ptgn {

class Scene;
class CameraManager;
class Camera;
struct Color;

namespace impl {

Camera CreateCamera(const Entity& entity);

class CameraInfo {
public:
	void SetViewportPosition(const V2_float& new_viewport_position);
	void SetViewportSize(const V2_float& new_viewport_size);
	// @param position Top left.
	void SetBoundingBox(const V2_float& new_bounding_position, const V2_float& new_bounding_size);

	void SetResizeToLogicalResolution(bool resize);
	void SetCenterOnLogicalResolution(bool center);

	void SetFlip(Flip flip);

	void SetPositionZ(float z);
	void SetRotationY(float rotation_y);
	void SetRotationZ(float rotation_z);

	void SetPixelRounding(bool enabled);

	[[nodiscard]] V2_float GetViewportPosition() const;
	[[nodiscard]] V2_float GetViewportSize() const;

	// @return Top left position.
	[[nodiscard]] V2_float GetBoundingBoxPosition() const;
	// @return Size of the bounding box.
	[[nodiscard]] V2_float GetBoundingBoxSize() const;

	[[nodiscard]] bool GetResizeToLogicalResolution() const;
	[[nodiscard]] bool GetCenterOnLogicalResolution() const;

	[[nodiscard]] Flip GetFlip() const;

	[[nodiscard]] float GetPositionZ() const;
	[[nodiscard]] float GetRotationY() const;
	[[nodiscard]] float GetRotationZ() const;

	[[nodiscard]] bool GetPixelRounding() const;

	[[nodiscard]] const Matrix4& GetViewProjection(const Transform& current, const Entity& entity)
		const;

	[[nodiscard]] const Matrix4& GetView(const Transform& current, const Entity& entity) const;
	[[nodiscard]] const Matrix4& GetProjection(const Transform& current) const;

	// Set the point which is at the center of the camera view.
	// void SetPosition(const V3_float& new_position);

	void RecalculateView(const Transform& current, const Transform& offset_transform) const;
	void RecalculateProjection(const Transform& current) const;
	void RecalculateViewProjection() const;

	[[nodiscard]] static V2_float ClampToBounds(
		V2_float position, const V2_float& bounding_box_position, const V2_float& bounding_box_size,
		const V2_float& viewport_size, const V2_float& camera_zoom
	);

	// TODO: Change this to recalculate view and projection matrices based on position instead of
	// storing it. This requires that the entity CameraInfo component is processed after Transform.
	// Later point might not be relevant after introducing Transform dirty flags.

	PTGN_SERIALIZER_REGISTER_IGNORE_DEFAULTS(
		CameraInfo, previous, view_dirty, projection_dirty, view, projection, view_projection,
		viewport_position, viewport_size, center_on_logical_resolution,
		resize_to_logical_resolution, pixel_rounding, bounding_box_position, bounding_box_size,
		flip, position_z, orientation_y, orientation_z
	)

	void SetViewDirty();

private:
	// Keep track of previous transform since the Transform dirty flags are only reset at the end of
	// the frame and the camera view or projection matrices may be requested multiple times in one
	// frame.
	mutable Transform previous;

	mutable bool view_dirty{ true };
	mutable bool projection_dirty{ true };

	// Mutable used because view projection is recalculated only upon retrieval to reduce matrix
	// multiplications.
	mutable Matrix4 view{ 1.0f };
	mutable Matrix4 projection{ 1.0f };
	mutable Matrix4 view_projection{ 1.0f };

	V2_float viewport_position;
	V2_float viewport_size;

	bool center_on_logical_resolution{ true };
	bool resize_to_logical_resolution{ true };

	// If true, rounds camera position to pixel precision.
	// TODO: Check that this works.
	bool pixel_rounding{ false };

	// Top left position.
	V2_float bounding_box_position;
	// If size is {}, no bounds are enforced.
	V2_float bounding_box_size;

	Flip flip{ Flip::None };

	float position_z{ 0.0f };
	float orientation_y{ 0.0f };
	float orientation_z{ 0.0f };
};

struct CameraResizeScript : public Script<CameraResizeScript, LogicalResolutionScript> {
	void OnLogicalResolutionChanged() override;
};

} // namespace impl

class Camera : public Entity {
public:
	// Get camera top left point.
	[[nodiscard]] V2_float GetTopLeft() const;
	/**
	 * @brief Translates the camera to a target position over a specified duration.
	 *
	 * @param target_position The position to move the camera's center to.
	 * @param duration The duration over which the translation should occur.
	 * @param ease The easing function to apply for the translation animation.
	 * @param force If true, forcibly overrides any ongoing translation.
	 */
	Camera& TranslateTo(
		const V2_float& target_position, milliseconds duration,
		const Ease& ease = SymmetricalEase::Linear, bool force = true
	);

	/**
	 * @brief Rotates the camera to a target angle over a specified duration.
	 *
	 * @param target_angle The angle (in radians) to rotate the camera to. Positive clockwise,
	 * negative counter-clockwise.
	 * @param duration The duration over which the rotation should occur.
	 * @param ease The easing function to apply for the rotation animation.
	 * @param force If true, forcibly overrides any ongoing rotation.
	 *            -1.5708
	 *               |
	 *    3.14159 ---o--- 0
	 *               |
	 *             1.5708
	 */
	Camera& RotateTo(
		float target_angle, milliseconds duration, const Ease& ease = SymmetricalEase::Linear,
		bool force = true
	);

	/**
	 * @brief Zooms the camera to a target zoom over a specified duration.
	 *
	 * @param target_zoom The target zoom (x, y) to apply to the camera.
	 * @param duration The duration over which the zooming should occur.
	 * @param ease The easing function to apply for the zoom animation.
	 * @param force If true, forcibly overrides any ongoing zooming.
	 */
	Camera& ZoomTo(
		const V2_float& target_zoom, milliseconds duration,
		const Ease& ease = SymmetricalEase::Linear, bool force = true
	);

	/**
	 * @brief Applies a continuous shake effect to the camera.
	 *
	 * @param intensity The intensity of the shake, in the range [0, 1].
	 * @param duration The total duration of the shake effect. If -1, the shake continues until
	 * StopShake is called.
	 * @param config Configuration parameters for the shake behavior.
	 * @param ease The easing function to use for the shake. If SymmetricalEase::None, shake remains
	 * at full intensity for the entire time.
	 * @param force If true, overrides any existing shake effect.
	 */
	Camera& Shake(
		float intensity, milliseconds duration, const ShakeConfig& config = {},
		const Ease& ease = SymmetricalEase::None, bool force = true
	);

	/**
	 * @brief Applies an instantenous shake effect to the camera.
	 *
	 * @param entity The entity to apply the shake effect to.
	 * @param intensity The intensity of the shake, in the range [0, 1].
	 * @param config Configuration parameters for the shake behavior.
	 * @param force If true, overrides any existing shake effect.
	 */
	Camera& Shake(float intensity, const ShakeConfig& config = {}, bool force = true);

	/**
	 * @brief Stops any ongoing camera shake.
	 *
	 * @param force If true, clears all queued or active shake effects.
	 */
	Camera& StopShake(bool force = true);

	Camera& StartFollow(Entity target, const FollowConfig& config = {}, bool force = true);

	Camera& StopFollow(bool force = false);

	/**
	 * @brief Fades the camera from its current tint to a color over a specified duration.
	 *
	 * @param target_color The target color to fade the camera to.
	 * @param duration The duration over which the fade should occur.
	 * @param ease The easing function to apply for the fade animation.
	 * @param force If true, forcibly overrides any ongoing fading.
	 */
	// TODO: Fix.
	// Camera& FadeTo(
	//	const Color& target_color, milliseconds duration,
	//	const Ease& ease = SymmetricalEase::Linear, bool force = true
	//);

	/**
	 * @brief Fades the camera from the specified color to transparent over a specified duration.
	 *
	 * @param start_color The color that the camera fades from.
	 * @param duration The duration over which the fade should occur.
	 * @param ease The easing function to apply for the fade animation.
	 * @param force If true, forcibly overrides any ongoing fading.
	 */
	// TODO: Fix.
	// Camera& FadeFrom(
	//	const Color& start_color, milliseconds duration, const Ease& ease = SymmetricalEase::Linear,
	//	bool force = true
	//);

	Camera() = default;
	Camera(const Entity& entity);

	void SetPixelRounding(bool enabled);
	[[nodiscard]] bool IsPixelRoundingEnabled() const;

	// Set the camera to be centered on the logical resolution.
	// Set the camera viewport to be equal to the logical resolution.
	void SetToLogicalResolution(bool continuously = true);

	// Set the camera to be centered on the logical resolution.
	void CenterOnLogicalResolution(bool continuously = false);

	// Set the camera viewport to be equal to the logical resolution.
	void SetViewportToLogicalResolution(bool continuously = false);

	[[nodiscard]] std::array<V2_float, 4> GetWorldVertices() const;

	[[nodiscard]] V2_float GetViewportPosition() const;
	[[nodiscard]] V2_float GetViewportSize() const;

	[[nodiscard]] V2_float GetZoom() const;

	[[nodiscard]] V2_float GetBoundsPosition() const;
	[[nodiscard]] V2_float GetBoundsSize() const;

	[[nodiscard]] Flip GetFlip() const;

	void SetFlip(Flip flip);

	// Camera bounds only apply along aligned axes. In other words: rotated cameras can see outside
	// the bounding box.
	// @param position Top left position of the bounds.
	void SetBounds(const V2_float& position, const V2_float& size);

	void SetViewportPosition(const V2_float& viewport_position);
	void SetViewportSize(const V2_float& viewport_size);

	void Translate(const V2_float& position_change);

	void SetZoom(float new_zoom);
	void SetZoom(V2_float new_zoom);

	void Zoom(float zoom_change_amount);
	void Zoom(const V2_float& zoom_change_amount);

	// (yaw, pitch, roll) in radians.
	// void SetRotation(const V3_float& new_angle_radians);

	// (yaw, pitch, roll) in radians.
	// void Rotate(const V3_float& angle_change_radians);

	// Set 2D rotation angle in radians.
	/* Range: (-3.14159, 3.14159].
	 * (clockwise positive).
	 *            -1.5708
	 *               |
	 *    3.14159 ---o--- 0
	 *               |
	 *             1.5708
	 */
	// void SetRotation(float angle_radians);

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
	// void SetYaw(float angle_radians);

	//// Angle in radians.
	// void Yaw(float angle_change_radians);

	//// Angle in radians.
	// void SetPitch(float angle_radians);

	//// Angle in radians.
	// void Pitch(float angle_change_radians);

	//// Angle in radians.
	// void SetRoll(float angle_radians);

	//// Angle in radians.
	// void Roll(float angle_change_radians);

	/*
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
	*/

	void Reset();

	void PrintInfo() const;

	[[nodiscard]] const Matrix4& GetViewProjection() const;

	operator Matrix4() const;

protected:
	friend struct impl::CameraResizeScript;
	friend class CameraManager;
	friend Camera impl::CreateCamera(const Entity& entity);

	// @return (yaw, pitch, roll) (radians).
	[[nodiscard]] V3_float GetOrientation() const;

	// Orientation as a quaternion.
	[[nodiscard]] Quaternion GetQuaternion() const;

	void SubscribeToLogicalResolutionEvents();
	void UnsubscribeFromLogicalResolutionEvents();

	// @param start_color Starting color.
	// @param end_color Ending color.
	// @param duration Duration of fade.
	// @param ease Easing function for the fade.
	// @param force If false, the fade is queued in the fade queue, if true the fade is executed
	// immediately, clearing any previously queued fades.
	/*Tween& FadeFromTo(
		const Color& start_color, const Color& end_color, milliseconds duration, const Ease& ease,
		bool force
	);*/

	void RefreshBounds();

	static void OnLogicalResolutionChanged(Camera camera, V2_float size);
};

inline std::ostream& operator<<(std::ostream& os, const ptgn::Camera& c) {
	os << "[center position: " << GetPosition(c)
	   << ", viewport position: " << c.GetViewportPosition()
	   << ", viewport size: " << c.GetViewportSize() << "]";
	return os;
}

class CameraManager {
public:
	void Reset();

	Camera primary;

	friend void to_json(json& j, const CameraManager& camera_manager);
	friend void from_json(const json& j, CameraManager& camera_manager);

private:
	friend class Scene;
	friend class impl::RenderData;

	Manager cameras_;

	void Init();
};

Camera CreateCamera(Manager& manager);
Camera CreateCamera(Scene& scene);

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
	// form of camera defined ENUM (to abstract it from other systems)
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