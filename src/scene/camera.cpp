#include "scene/camera.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <type_traits>

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
#include "renderer/renderer.h"
#include "utility/debug.h"
#include "utility/log.h"

namespace ptgn {

Camera::Camera() {
	center_to_window = true;
	resize_to_window = true;
	SubscribeToEvents();
}

Camera::Camera(const Camera& copy) {
	*this = copy;
}

Camera& Camera::operator=(const Camera& copy) noexcept {
	this->position				 = copy.position;
	this->size					 = copy.size;
	this->zoom					 = copy.zoom;
	this->orientation			 = copy.orientation;
	this->bounding_box			 = copy.bounding_box;
	this->flip					 = copy.flip;
	this->view					 = copy.view;
	this->projection			 = copy.projection;
	this->view_projection		 = copy.view_projection;
	this->recalculate_view		 = copy.recalculate_view;
	this->recalculate_projection = copy.recalculate_projection;
	this->center_to_window		 = copy.center_to_window;
	this->resize_to_window		 = copy.resize_to_window;
	if (game.event.window.IsSubscribed(&copy)) {
		SubscribeToEvents();
	}
	return *this;
}

Camera::Camera(Camera&& move) noexcept {
	*this = move;
}

Camera& Camera::operator=(Camera&& move) noexcept {
	this->position				 = std::move(move.position);
	this->size					 = std::move(move.size);
	this->zoom					 = move.zoom;
	this->orientation			 = std::move(move.orientation);
	this->bounding_box			 = std::move(move.bounding_box);
	this->flip					 = move.flip;
	this->view					 = std::move(move.view);
	this->projection			 = std::move(move.projection);
	this->view_projection		 = std::move(move.view_projection);
	this->recalculate_view		 = move.recalculate_view;
	this->recalculate_projection = move.recalculate_projection;
	this->center_to_window		 = move.center_to_window;
	this->resize_to_window		 = move.resize_to_window;
	if (game.event.window.IsSubscribed(&move)) {
		SubscribeToEvents();
		game.event.window.Unsubscribe(&move);
	}
	return *this;
}

Camera::~Camera() {
	game.event.window.Unsubscribe(this);
}

void Camera::SubscribeToEvents() {
	std::function<void(const WindowResizedEvent& e)> f = [this](const WindowResizedEvent& e) {
		OnWindowResize(e);
	};
	game.event.window.Subscribe(WindowEvent::Resized, this, f);
	std::invoke(f, WindowResizedEvent{ game.window.GetSize() });
}

void Camera::OnWindowResize(const WindowResizedEvent& e) {
	if (!game.event.window.IsSubscribed(this)) {
		return;
	}
	if (resize_to_window) {
		size				   = e.size;
		recalculate_projection = true;
	}
	if (center_to_window) {
		position.x		 = static_cast<float>(e.size.x) / 2.0f;
		position.y		 = static_cast<float>(e.size.y) / 2.0f;
		recalculate_view = true;
	}
	if (resize_to_window || center_to_window) {
		RefreshBounds();
	}
}

Camera::operator Matrix4() const {
	return GetViewProjection();
}

void Camera::RefreshBounds() {
	if (bounding_box.IsZero()) {
		return;
	}
	V2_float min{ bounding_box.Min() };
	V2_float max{ bounding_box.Max() };
	PTGN_ASSERT(min.x < max.x && min.y < max.y, "Bounding box min must be below maximum");
	V2_float center{ bounding_box.Center() };
	// Draw bounding box center.
	// game.draw.Point(center, color::Red, 5.0f);
	// Draw bounding box.
	// game.draw.RectHollow(bounding_box, color::Red);

	// TODO: Incoporate yaw, i.e. orientation.x into the bounds using sin and cos.
	V2_float zoom_size{ size / zoom };
	V2_float half{ zoom_size * 0.5f };
	if (zoom_size.x > bounding_box.size.x) {
		position.x = center.x;
	} else {
		position.x = std::clamp(position.x, min.x + half.x, max.x - half.x);
	}
	if (zoom_size.y > bounding_box.size.y) {
		position.y = center.y;
	} else {
		position.y = std::clamp(position.y, min.y + half.y, max.y - half.y);
	}
	recalculate_view = true;
}

void Camera::SetZoom(float new_zoom) {
	zoom = new_zoom;
	zoom = std::clamp(new_zoom, epsilon<float>, std::numeric_limits<float>::max());
	recalculate_projection = true;
	RefreshBounds();
}

void Camera::SetSize(const V2_float& new_size) {
	resize_to_window	   = false;
	size				   = new_size;
	recalculate_projection = true;
	RefreshBounds();
}

void Camera::SetPosition(const V3_float& new_position) {
	center_to_window = false;
	position		 = new_position;
	recalculate_view = true;
	RefreshBounds();
}

void Camera::SetBounds(const Rect& new_bounding_box) {
	bounding_box = new_bounding_box;
	// Reset position to ensure it is within the new bounds.
	RefreshBounds();
}

void Camera::Reset() {
	position	 = {};
	size		 = {};
	zoom		 = 1.0f;
	orientation	 = {};
	bounding_box = {};
	flip		 = Flip::None;

	view			= Matrix4{ 1.0f };
	projection		= Matrix4{ 1.0f };
	view_projection = Matrix4{ 1.0f };

	recalculate_view	   = false;
	recalculate_projection = false;
	center_to_window	   = true;
	resize_to_window	   = true;
}

Rect Camera::GetBounds() const {
	return bounding_box;
}

V2_float Camera::GetPosition(Origin origin) const {
	return V2_float{ position.x, position.y } + GetOffsetFromCenter(size, origin);
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

void Camera::CenterOnWindow(bool continuously) {
	if (continuously) {
		center_to_window = true;
		SubscribeToEvents();
	} else {
		SetPosition(game.window.GetCenter());
	}
}

V2_float Camera::ScreenToCamera(const V2_float& screen_coordinate) const {
	return (screen_coordinate - GetSize() * 0.5f) / GetZoom() + GetPosition();
}

Rect Camera::GetRect() const {
	return Rect{ GetPosition(Origin::TopLeft), GetSize(), Origin::TopLeft };
}

V2_float Camera::GetSize() const {
	return size;
}

float Camera::GetZoom() const {
	return zoom;
}

V3_float Camera::GetOrientation() const {
	return orientation;
}

Quaternion Camera::GetQuaternion() const {
	return Quaternion::FromEuler(orientation);
}

Flip Camera::GetFlip() const {
	return flip;
}

void Camera::SetFlip(Flip new_flip) {
	flip = new_flip;
}

const Matrix4& Camera::GetView() const {
	if (recalculate_view) {
		RecalculateView();
	}
	return view;
}

const Matrix4& Camera::GetProjection() const {
	if (recalculate_projection) {
		RecalculateProjection();
	}
	return projection;
}

const Matrix4& Camera::GetViewProjection() const {
	bool updated_matrix{ recalculate_view || recalculate_projection };
	if (recalculate_view) {
		RecalculateView();
		recalculate_view = false;
	}
	if (recalculate_projection) {
		RecalculateProjection();
		recalculate_projection = false;
	}
	if (updated_matrix) {
		RecalculateViewProjection();
	}
	return view_projection;
}

void Camera::SetPosition(const V2_float& new_position) {
	SetPosition({ new_position.x, new_position.y, position.z });
}

void Camera::Translate(const V2_float& position_change) {
	SetPosition(
		position + V3_float{ position_change.x, position_change.y, 0.0f } * GetQuaternion()
	);
}

void Camera::Zoom(float zoom_change) {
	SetZoom(zoom + zoom_change);
}

void Camera::SetRotation(const V3_float& new_angle_radians) {
	orientation		 = new_angle_radians;
	recalculate_view = true;
}

void Camera::Rotate(const V3_float& angle_change_radians) {
	SetRotation(orientation + angle_change_radians);
}

void Camera::SetRotation(float yaw_radians) {
	SetYaw(yaw_radians);
}

void Camera::Rotate(float yaw_change_radians) {
	Yaw(yaw_change_radians);
}

void Camera::SetYaw(float angle_radians) {
	orientation.x = angle_radians;
}

void Camera::SetPitch(float angle_radians) {
	orientation.y = angle_radians;
}

void Camera::SetRoll(float angle_radians) {
	orientation.z = angle_radians;
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
		resize_to_window = true;
		SubscribeToEvents();
	} else {
		SetSize(game.window.GetSize());
	}
}

void Camera::RecalculateViewProjection() const {
	view_projection = projection * view;
}

void Camera::RecalculateView() const {
	V3_float mirror_position{ -position.x, -position.y, position.z };

	Quaternion quat_orientation{ GetQuaternion() };
	view = Matrix4::Translate(quat_orientation.ToMatrix4(), mirror_position);
}

void Camera::RecalculateProjection() const {
	PTGN_ASSERT(zoom > 0.0f);
	V2_float extents{ size / 2.0f / zoom };
	V2_float flip_dir{ 1.0f, 1.0f };
	switch (flip) {
		case Flip::None:	   break;
		case Flip::Vertical:   flip_dir.y = -1.0f; break;
		case Flip::Horizontal: flip_dir.x = -1.0f; break;
		case Flip::Both:
			flip_dir.x = -1.0f;
			flip_dir.y = -1.0f;
			break;
		default: PTGN_ERROR("Unrecognized flip state");
	}
	projection = Matrix4::Orthographic(
		flip_dir.x * -extents.x, flip_dir.x * extents.x, flip_dir.y * extents.y,
		flip_dir.y * -extents.y, -std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::infinity()
	);
}

void Camera::PrintInfo() const {
	auto bounds{ GetBounds() };
	auto orient{ GetOrientation() };
	Print(
		"Position: ", GetPosition(), ", Size: ", GetSize(), ", Zoom: ", GetZoom(),
		", Orientation (yaw/pitch/roll) (deg): (", RadToDeg(orient.x), ", ", RadToDeg(orient.y),
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
	SetPrimary({});
}

void CameraManager::SetPrimary(const Camera& camera) {
	primary_camera_ = camera;
	CameraChanged();
}

const Camera& CameraManager::GetPrimary() const {
	return primary_camera_;
}

void CameraManager::SetPrimaryImpl(const InternalKey& key) {
	PTGN_ASSERT(Has(key), "Cannot set camera which has not been loaded as the primary camera");
	SetPrimary(Get(key));
}

void CameraManager::Reset() {
	MapManager::Reset();
	SetPrimary({});
}

void CameraManager::CameraChanged() const {
	if (auto current_target{ game.renderer.GetRenderTarget() }; current_target.IsValid()) {
		current_target.SetCamera(primary_camera_);
	}
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

			V2_float size = game.window.GetSize();

			V2_float scaled_offset = offset / size;

			// OpenGL y-axis is flipped compared to SDL mouse position.
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

V2_float ScreenToViewport(
	const Rect& viewport, const Camera& camera, const V2_float& screen_coordinate
) {
	return (camera.ScreenToCamera(screen_coordinate) - viewport.Min()) * camera.GetSize() /
		   viewport.size;
}

} // namespace ptgn