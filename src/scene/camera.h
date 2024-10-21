#pragma once

#include <map>

#include "core/manager.h"
#include "math/geometry/polygon.h"
#include "math/matrix4.h"
#include "math/quaternion.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "renderer/flip.h"
#include "utility/handle.h"
#include "utility/type_traits.h"

namespace ptgn {

class Game;
class OrthographicCamera;
class Renderer;

V2_float WorldToScreen(const V2_float& position, std::size_t render_layer = 0);
V2_float ScaleToScreen(const V2_float& size, std::size_t render_layer = 0);
float ScaleToScreen(float size, std::size_t render_layer = 0);
V2_float ScreenToWorld(const V2_float& position, std::size_t render_layer = 0);
V2_float ScaleToWorld(const V2_float& size, std::size_t render_layer = 0);
float ScaleToWorld(float size, std::size_t render_layer = 0);

namespace impl {

struct Camera {
	V3_float position;
	V2_float size;
	float zoom{ 1.0f };
	V3_float orientation;

	~Camera();
	void Reset();

	// If rectangle IsZero(), no position bounds are enforced.
	Rect bounding_box;

	Flip flip{ Flip::None };

	M4_float view{ 1.0f };
	M4_float projection{ 1.0f };
	M4_float view_projection{ 1.0f };

	bool recalculate_view{ false };
	bool recalculate_projection{ false };
	bool center_to_window{ true };
	bool resize_to_window{ true };
};

} // namespace impl

class OrthographicCamera : public Handle<impl::Camera> {
public:
	OrthographicCamera()		   = default;
	~OrthographicCamera() override = default;

	// Set the camera to be centered on the window.
	void SetToWindow();
	void CenterOnArea(const V2_float& size);

	// Origin at the top left.
	[[nodiscard]] Rect GetRectangle() const;
	[[nodiscard]] V2_float GetTopLeftPosition() const;
	[[nodiscard]] V2_float GetSize() const;
	[[nodiscard]] float GetZoom() const;
	// Use Min() and Max() of rectangle to find top left and bottom right bounds of camera.
	[[nodiscard]] Rect GetBounds() const;
	[[nodiscard]] V2_float GetPosition() const;
	[[nodiscard]] V3_float GetPosition3D() const;
	// (yaw, pitch, roll) (radians).
	[[nodiscard]] V3_float GetOrientation() const;
	// Orientation as a quaternion.
	[[nodiscard]] Quaternion GetQuaternion() const;
	[[nodiscard]] Flip GetFlip() const;

	void SetFlip(Flip flip);

	// If continuously is true, camera will subscribe to window resize event.
	void CenterOnWindow(bool continuously = false);

	void SetBounds(const Rect& bounding_box);

	// If continuously is true, camera will subscribe to window resize event.
	void SetSizeToWindow(bool continuously = false);
	void SetSize(const V2_float& size);

	void SetPosition(const V3_float& new_position);
	void Translate(const V3_float& position_change);
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

	[[nodiscard]] const M4_float& GetViewProjection();

protected:
	void RefreshBounds();

	void SetPositionImpl(const V3_float& new_position);
	void SetSizeImpl(const V2_float& size);

	[[nodiscard]] const M4_float& GetView();
	[[nodiscard]] const M4_float& GetProjection();

	void RecalculateView();
	void RecalculateProjection();
	void RecalculateViewProjection();

	void SubscribeToWindowResize();
	void UnsubscribeFromWindowResize() const;
};

namespace impl {

class Game;
class Renderer;
class SceneCamera;

class CameraManager : public MapManager<OrthographicCamera> {
public:
	using MapManager::MapManager;

	CameraManager();

	template <typename TKey>
	void SetPrimary(const TKey& key, std::size_t render_layer = 0) {
		SetPrimaryImpl(GetInternalKey(key), render_layer);
	}

	void SetPrimary(const OrthographicCamera& camera, std::size_t render_layer = 0);

	[[nodiscard]] const OrthographicCamera& GetPrimary(std::size_t render_layer = 0) const;

	// If primary camera does not exist for the given layer, it will be
	[[nodiscard]] OrthographicCamera& GetPrimary(std::size_t render_layer = 0);

	void Reset();

	// Resets all render layer primary cameras.
	void ResetPrimary();

private:
	friend class SceneCamera;
	friend class Game;
	friend class Renderer;

	void SetPrimaryImpl(const InternalKey& key, std::size_t render_layer);

	// Key: render_layer, Value: camera.
	std::map<std::size_t, OrthographicCamera> primary_cameras_;
};

// This class provides quick access to the current top active scene.
// i.e. using this is class equivalent to game.scene.GetTopActive().camera
class SceneCamera {
public:
	SceneCamera()											 = default;
	~SceneCamera()											 = default;
	SceneCamera(const SceneCamera&)			 = delete;
	SceneCamera(SceneCamera&&)				 = default;
	SceneCamera& operator=(const SceneCamera&) = delete;
	SceneCamera& operator=(SceneCamera&&)		 = default;

	using Item = CameraManager::Item;

	template <typename TKey, typename... TArgs, tt::constructible<Item, TArgs...> = true>
	static Item& Load(const TKey& key, TArgs&&... constructor_args) {
		return LoadImpl(CameraManager::GetInternalKey(key), std::move(Item(constructor_args...)));
	}

	template <typename TKey>
	static void Unload(const TKey& key) {
		UnloadImpl(CameraManager::GetInternalKey(key));
	}

	template <typename TKey>
	[[nodiscard]] static bool Has(const TKey& key) {
		return HasImpl(CameraManager::GetInternalKey(key));
	}

	template <typename TKey>
	[[nodiscard]] static Item& Get(const TKey& key) {
		return GetImpl(CameraManager::GetInternalKey(key));
	}

	static void Clear();

	template <typename TKey>
	static void SetPrimary(const TKey& key, std::size_t render_layer = 0) {
		SetPrimaryImpl(CameraManager::GetInternalKey(key), render_layer);
	}

	static void SetPrimary(const OrthographicCamera& camera, std::size_t render_layer = 0);

	[[nodiscard]] const OrthographicCamera& GetPrimary(std::size_t render_layer = 0) const;
	[[nodiscard]] OrthographicCamera& GetPrimary(std::size_t render_layer = 0);

	static void Reset();
	static void ResetPrimary();

private:
	friend class Game;

	using InternalKey = CameraManager::InternalKey;

	static Item& LoadImpl(const InternalKey& key, Item&& item);
	static void UnloadImpl(const InternalKey& key);
	[[nodiscard]] static bool HasImpl(const InternalKey& key);
	[[nodiscard]] static Item& GetImpl(const InternalKey& key);

	static void SetPrimaryImpl(const InternalKey& key, std::size_t render_layer);
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
	M4_float GetViewMatrix() const {
		return M4_float::LookAt(position, position + front, up);
	}

	M4_float GetProjectionMatrix() const {
		return projection;
	}

	void SetProjectionMatrix(const M4_float& p) {
		projection = p;
	}

	M4_float projection{ 1.0f };

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