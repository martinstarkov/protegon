#include "scene/camera.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>

#include "core/game.h"
#include "core/manager.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "math/geometry/polygon.h"
#include "math/math.h"
#include "math/matrix4.h"
#include "math/quaternion.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "renderer/flip.h"
#include "renderer/origin.h"
#include "utility/assert.h"
#include "utility/log.h"

namespace ptgn {

Camera::Camera() {
	info.center_to_window = true;
	info.resize_to_window = true;
	SubscribeToEvents();
}

Camera::Camera(const Camera& other) {
	*this = other;
}

Camera& Camera::operator=(const Camera& other) {
	info = other.info;
	if (game.event.window.IsSubscribed(&other) && !game.event.window.IsSubscribed(this)) {
		SubscribeToEvents();
	}
	return *this;
}

Camera::Camera(Camera&& other) noexcept : info{ other.info } {
	if (game.event.window.IsSubscribed(&other)) {
		SubscribeToEvents();
		game.event.window.Unsubscribe(&other);
	}
}

Camera& Camera::operator=(Camera&& other) noexcept {
	if (this != &other) {
		info = std::exchange(other.info, {});
		if (game.event.window.IsSubscribed(&other)) {
			if (!game.event.window.IsSubscribed(this)) {
				SubscribeToEvents();
			}
			game.event.window.Unsubscribe(&other);
		} else {
			game.event.window.Unsubscribe(this);
		}
	}
	return *this;
}

Camera::~Camera() {
	game.event.window.Unsubscribe(this);
}

void Camera::SubscribeToEvents() noexcept {
	std::function<void(const WindowResizedEvent& e)> f = [this](const WindowResizedEvent& e) {
		OnWindowResize(e);
	};
	game.event.window.Subscribe(WindowEvent::Resized, this, f);
	std::invoke(f, WindowResizedEvent{ game.window.GetSize() });
}

void Camera::OnWindowResize(const WindowResizedEvent& e) noexcept {
	if (!game.event.window.IsSubscribed(this)) {
		return;
	}
	if (info.resize_to_window) {
		info.size					= e.size;
		info.recalculate_projection = true;
	}
	if (info.center_to_window) {
		info.position.x		  = static_cast<float>(e.size.x) / 2.0f;
		info.position.y		  = static_cast<float>(e.size.y) / 2.0f;
		info.recalculate_view = true;
	}
	if (info.resize_to_window || info.center_to_window) {
		RefreshBounds();
	}
}

Camera::operator Matrix4() const {
	return GetViewProjection();
}

void Camera::RefreshBounds() noexcept {
	if (info.bounding_box.IsZero()) {
		return;
	}
	V2_float min{ info.bounding_box.Min() };
	V2_float max{ info.bounding_box.Max() };
	PTGN_ASSERT(min.x < max.x && min.y < max.y, "Bounding box min must be below maximum");
	V2_float center{ info.bounding_box.Center() };
	// Draw bounding box center.
	// game.draw.Point(center, color::Red, 5.0f);
	// Draw bounding box.
	// game.draw.RectHollow(info.bounding_box, color::Red);

	// TODO: Incoporate yaw, i.e. info.orientation.x into the bounds using sin and cos.
	V2_float size{ info.size / info.zoom };
	V2_float half{ info.size * 0.5f };
	if (info.size.x > info.bounding_box.size.x) {
		info.position.x = center.x;
	} else {
		info.position.x = std::clamp(info.position.x, min.x + half.x, max.x - half.x);
	}
	if (info.size.y > info.bounding_box.size.y) {
		info.position.y = center.y;
	} else {
		info.position.y = std::clamp(info.position.y, min.y + half.y, max.y - half.y);
	}
	info.recalculate_view = true;
}

void Camera::SetZoom(float new_zoom) {
	info.zoom = new_zoom;
	info.zoom = std::clamp(info.zoom, epsilon<float>, std::numeric_limits<float>::max());
	info.recalculate_projection = true;
	RefreshBounds();
}

void Camera::SetSize(const V2_float& new_size) {
	info.resize_to_window		= false;
	info.size					= new_size;
	info.recalculate_projection = true;
	RefreshBounds();
}

void Camera::SetPosition(const V3_float& new_position) {
	info.center_to_window = false;
	info.position		  = new_position;
	info.recalculate_view = true;
	RefreshBounds();
}

void Camera::SetBounds(const Rect& new_bounding_box) {
	info.bounding_box = new_bounding_box;
	// Reset info.position to ensure it is within the new bounds.
	RefreshBounds();
}

void Camera::Reset() {
	info = {};
}

Rect Camera::GetBounds() const {
	return info.bounding_box;
}

V2_float Camera::GetPosition(Origin origin) const {
	return V2_float{ info.position.x, info.position.y } + GetOffsetFromCenter(info.size, origin);
}

void Camera::SetToWindow(bool continuously) {
	if (continuously) {
		game.event.window.Unsubscribe(this);
	}
	Reset();
	CenterOnWindow(continuously);
	SetSizeToWindow(continuously);
}

void Camera::CenterOnArea(const V2_float& new_size) {
	SetSize(new_size);
	SetPosition(new_size / 2.0f);
}

V2_float Camera::TransformToCamera(const V2_float& screen_relative_coordinate) const {
	PTGN_ASSERT(info.zoom != 0.0f);
	return (screen_relative_coordinate - info.size * 0.5f) / info.zoom + GetPosition();
}

V2_float Camera::TransformToScreen(const V2_float& camera_relative_coordinate) const {
	return (camera_relative_coordinate - GetPosition()) * info.zoom + info.size * 0.5f;
}

V2_float Camera::ScaleToCamera(const V2_float& screen_relative_size) const {
	return screen_relative_size * info.zoom;
}

V2_float Camera::ScaleToScreen(const V2_float& camera_relative_size) const {
	PTGN_ASSERT(info.zoom != 0.0f);
	return camera_relative_size / info.zoom;
}

void Camera::CenterOnWindow(bool continuously) {
	if (continuously) {
		info.center_to_window = true;
		SubscribeToEvents();
	} else {
		SetPosition(game.window.GetCenter());
	}
}

Rect Camera::GetRect() const {
	return Rect{ GetPosition(Origin::Center), GetSize() / info.zoom, Origin::Center };
}

V2_float Camera::GetSize() const {
	return info.size;
}

float Camera::GetZoom() const {
	return info.zoom;
}

V3_float Camera::GetOrientation() const {
	return info.orientation;
}

Quaternion Camera::GetQuaternion() const {
	return Quaternion::FromEuler(info.orientation);
}

Flip Camera::GetFlip() const {
	return info.flip;
}

void Camera::SetFlip(Flip new_flip) {
	info.flip = new_flip;
}

const Matrix4& Camera::GetView() const {
	if (info.recalculate_view) {
		RecalculateView();
	}
	return info.view;
}

const Matrix4& Camera::GetProjection() const {
	if (info.recalculate_projection) {
		RecalculateProjection();
	}
	return info.projection;
}

const Matrix4& Camera::GetViewProjection() const {
	bool updated_matrix{ info.recalculate_view || info.recalculate_projection };
	if (info.recalculate_view) {
		RecalculateView();
		info.recalculate_view = false;
	}
	if (info.recalculate_projection) {
		RecalculateProjection();
		info.recalculate_projection = false;
	}
	if (updated_matrix) {
		RecalculateViewProjection();
	}
	return info.view_projection;
}

void Camera::SetPosition(const V2_float& new_position) {
	SetPosition({ new_position.x, new_position.y, info.position.z });
}

void Camera::Translate(const V2_float& position_change) {
	SetPosition(
		info.position + V3_float{ position_change.x, position_change.y, 0.0f } * GetQuaternion()
	);
}

void Camera::Zoom(float zoom_change) {
	SetZoom(info.zoom + zoom_change);
}

void Camera::SetRotation(const V3_float& new_angle_radians) {
	info.orientation	  = new_angle_radians;
	info.recalculate_view = true;
}

void Camera::Rotate(const V3_float& angle_change_radians) {
	SetRotation(info.orientation + angle_change_radians);
}

void Camera::SetRotation(float yaw_radians) {
	SetYaw(yaw_radians);
}

void Camera::Rotate(float yaw_change_radians) {
	Yaw(yaw_change_radians);
}

void Camera::SetYaw(float angle_radians) {
	info.orientation.x = angle_radians;
}

void Camera::SetPitch(float angle_radians) {
	info.orientation.y = angle_radians;
}

void Camera::SetRoll(float angle_radians) {
	info.orientation.z = angle_radians;
}

void Camera::Yaw(float angle_change) {
	Rotate({ angle_change, 0.0f, 0.0f });
}

void Camera::Pitch(float angle_change) {
	Rotate({ 0.0f, angle_change, 0.0f });
}

void Camera::Roll(float angle_change) {
	Rotate({ 0.0f, 0.0f, angle_change });
}

void Camera::SetSizeToWindow(bool continuously) {
	if (continuously) {
		info.resize_to_window = true;
		SubscribeToEvents();
	} else {
		SetSize(game.window.GetSize());
	}
}

void Camera::RecalculateViewProjection() const {
	info.view_projection = info.projection * info.view;
}

void Camera::RecalculateView() const {
	V3_float mirror_position{ -info.position.x, -info.position.y, info.position.z };

	Quaternion quat_orientation{ GetQuaternion() };
	info.view = Matrix4::Translate(quat_orientation.ToMatrix4(), mirror_position);
}

void Camera::RecalculateProjection() const {
	PTGN_ASSERT(info.zoom > 0.0f);
	V2_float extents{ info.size / 2.0f / info.zoom };
	V2_float flip_dir{ 1.0f, 1.0f };
	switch (info.flip) {
		case Flip::None:	   break;
		case Flip::Vertical:   flip_dir.y = -1.0f; break;
		case Flip::Horizontal: flip_dir.x = -1.0f; break;
		case Flip::Both:
			flip_dir.x = -1.0f;
			flip_dir.y = -1.0f;
			break;
		default: PTGN_ERROR("Unrecognized info.flip state");
	}
	info.projection = Matrix4::Orthographic(
		flip_dir.x * -extents.x, flip_dir.x * extents.x, flip_dir.y * extents.y,
		flip_dir.y * -extents.y, -std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity()
	);
}

void Camera::PrintInfo() const {
	auto bounds{ GetBounds() };
	auto orient{ GetOrientation() };
	Print(
		"position: ", GetPosition(), ", size: ", GetSize(), ", zoom: ", GetZoom(),
		", orientation (yaw/pitch/roll) (deg): (", RadToDeg(orient.x), ", ", RadToDeg(orient.y),
		", ", RadToDeg(orient.z), "), Bounds: "
	);
	if (bounds.IsZero()) {
		PrintLine("none");
	} else {
		PrintLine(bounds.Min(), "->", bounds.Max());
	}
}

namespace impl {

void CameraManager::Init() {
	primary = {};
}

void CameraManager::Reset() {
	MapManager::Reset();
	primary = {};
}

} // namespace impl

/*
// To move camera according to mouse drag (in 3D):
void CameraController::OnMouseMoveEvent([[maybe_unused]] const MouseMoveEvent& e) {

	static bool first_mouse = true;

	if (game.input.MousePressed(Mouse::Left)) {
		const MouseMoveEvent& mouse = static_cast<const MouseMoveEvent&>(e);
		if (!first_mouse) {
			V2_float offset = mouse.GetDifference();

			V2_float info.size = game.window.GetSize();

			V2_float scaled_offset = offset / info.size;

			// OpenGL y-axis is info.flipped compared to SDL mouse info.position.
			Rotate(scaled_offset.x, -scaled_offset.y, 0.0f);
		} else {
			first_mouse = false;
		}
	}
}

void CameraController::SubscribeToMouseEvents() {
	game.event.mouse.Subscribe(MouseEvent::Move, this, std::function([&](const
MouseMoveEvent& e) { OnMouseMoveEvent(e);
	}));
}

void CameraController::UnsubscribeFromMouseEvents() {
	game.event.mouse.Unsubscribe(this);
}
*/

V2_float TransformToViewport(
	const Rect& viewport, const Camera& camera, const V2_float& screen_relative_coordinate
) {
	PTGN_ASSERT(viewport.size.x != 0.0f && viewport.size.y != 0.0f);
	return (camera.TransformToCamera(screen_relative_coordinate) - viewport.Min()) *
		   camera.GetSize() / viewport.size;
}

V2_float TransformToScreen(
	const Rect& viewport, const Camera& camera, const V2_float& viewport_relative_coordinate
) {
	V2_float cam_size{ camera.GetSize() };
	PTGN_ASSERT(cam_size.x != 0.0f && cam_size.y != 0.0f);
	return camera.TransformToScreen(
		viewport_relative_coordinate * viewport.size / cam_size + viewport.Min()
	);
}

V2_float ScaleToViewport(
	const Rect& viewport, const Camera& camera, const V2_float& screen_relative_size
) {
	PTGN_ASSERT(viewport.size.x != 0.0f && viewport.size.y != 0.0f);
	return (camera.ScaleToCamera(screen_relative_size)) * camera.GetSize() / viewport.size;
}

V2_float ScaleToScreen(
	const Rect& viewport, const Camera& camera, const V2_float& viewport_relative_size
) {
	V2_float cam_size{ camera.GetSize() };
	PTGN_ASSERT(cam_size.x != 0.0f && cam_size.y != 0.0f);
	return camera.ScaleToScreen(viewport_relative_size) * viewport.size / cam_size;
}

} // namespace ptgn