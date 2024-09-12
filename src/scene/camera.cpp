#include "camera.h"

#include "core/window.h"
#include "protegon/game.h"

namespace ptgn {

namespace impl {

Camera::~Camera() {
	if (game.event.window.IsSubscribed((void*)this)) {
		game.event.window.Unsubscribe((void*)this);
	}
}

} // namespace impl

Rectangle<float> OrthographicCamera::GetBounds() const {
	PTGN_ASSERT(IsValid());
	return instance_->bounding_box;
}

V2_float OrthographicCamera::GetPosition() const {
	PTGN_ASSERT(IsValid());
	return { instance_->position.x, instance_->position.y };
}

V3_float OrthographicCamera::GetPosition3D() const {
	PTGN_ASSERT(IsValid());
	return instance_->position;
}

Rectangle<float> OrthographicCamera::GetRectangle() const {
	return Rectangle<float>{ GetTopLeftPosition(), GetSize(), Origin::TopLeft };
}

V2_float OrthographicCamera::GetTopLeftPosition() const {
	return GetPosition() - GetSize() / 2.0f;
}

V2_float OrthographicCamera::GetSize() const {
	PTGN_ASSERT(IsValid());
	return instance_->size;
}

float OrthographicCamera::GetZoom() const {
	PTGN_ASSERT(IsValid());
	return instance_->zoom;
}

V3_float OrthographicCamera::GetOrientation() const {
	PTGN_ASSERT(IsValid());
	return instance_->orientation;
}

Quaternion OrthographicCamera::GetQuaternion() const {
	PTGN_ASSERT(IsValid());
	return Quaternion::FromEuler(instance_->orientation);
}

void OrthographicCamera::CenterOnWindow(bool continuously) {
	if (continuously) {
		instance_->center_to_window = true;
		SubscribeToWindowResize();
	} else {
		SetSize(game.window.GetSize());
	}
}

void OrthographicCamera::SubscribeToWindowResize() {
	CreateInstance();
	if (!game.event.window.IsSubscribed((void*)instance_.get())) {
		game.event.window.Subscribe(
			WindowEvent::Resized, (void*)instance_.get(),
			std::function([=](const WindowResizedEvent& e) { OnWindowResize(e.size); })
		);
	}
	OnWindowResize(game.window.GetSize());
}

void OrthographicCamera::UnsubscribeFromWindowResize() {
	if (IsValid() && game.event.window.IsSubscribed((void*)instance_.get())) {
		game.event.window.Unsubscribe((void*)instance_.get());
	}
}

const M4_float& OrthographicCamera::GetView() {
	PTGN_ASSERT(IsValid());
	if (instance_->recalculate_view) {
		RecalculateView();
	}
	return instance_->view;
}

const M4_float& OrthographicCamera::GetProjection() {
	PTGN_ASSERT(IsValid());
	if (instance_->recalculate_projection) {
		RecalculateProjection();
	}
	return instance_->projection;
}

const M4_float& OrthographicCamera::GetViewProjection() {
	PTGN_ASSERT(IsValid());
	bool updated_matrix{ instance_->recalculate_view || instance_->recalculate_projection };
	if (instance_->recalculate_view) {
		RecalculateView();
		instance_->recalculate_view = false;
	}
	if (instance_->recalculate_projection) {
		RecalculateProjection();
		instance_->recalculate_projection = false;
	}
	if (updated_matrix) {
		RecalculateViewProjection();
	}
	return instance_->view_projection;
}

void OrthographicCamera::OnWindowResize(const V2_float& size) {
	PTGN_ASSERT(IsValid());
	if (instance_->resize_to_window) {
		SetSizeImpl(size);
	}
	if (instance_->center_to_window) {
		SetPositionImpl({ size.x / 2.0f, size.y / 2.0f, instance_->position.z });
	}
}

void OrthographicCamera::CreateInstance() {
	if (!IsValid()) {
		instance_ = std::make_shared<impl::Camera>();
	}
}

void OrthographicCamera::SetPosition(const V2_float& new_position) {
	CreateInstance();
	SetPosition({ new_position.x, new_position.y, instance_->position.z });
}

void OrthographicCamera::RefreshBounds() {
	if (!instance_->bounding_box.IsZero()) {
		V2_float min{ instance_->bounding_box.Min() };
		V2_float max{ instance_->bounding_box.Max() };
		PTGN_ASSERT(min.x < max.x && min.y < max.y, "Bounding box min must be below maximum");
		V2_float center{ instance_->bounding_box.Center() };
		// Draw bounding box center.
		// game.renderer.DrawPoint(center, color::Red, 5.0f);
		// Draw bounding box.
		// game.renderer.DrawRectangleHollow(instance_->bounding_box, color::Red);
		V2_float size{ instance_->size / instance_->zoom };
		V2_float half{ size * 0.5f };
		if (size.x > instance_->bounding_box.size.x) {
			instance_->position.x = center.x;
		} else {
			instance_->position.x =
				std::clamp(instance_->position.x, min.x + half.x, max.x - half.x);
		}
		if (size.y > instance_->bounding_box.size.y) {
			instance_->position.y = center.y;
		} else {
			instance_->position.y =
				std::clamp(instance_->position.y, min.y + half.y, max.y - half.y);
		}
		// Draw clamped camera position.
		/*game.renderer.DrawPoint(
			{ instance_->position.x, instance_->position.y }, color::Yellow, 5.0f
		);*/
		instance_->recalculate_view = true;
	}
}

void OrthographicCamera::SetPositionImpl(const V3_float& new_position) {
	PTGN_ASSERT(IsValid());
	instance_->position			= new_position;
	instance_->recalculate_view = true;
	RefreshBounds();
}

void OrthographicCamera::SetPosition(const V3_float& new_position) {
	CreateInstance();
	instance_->center_to_window = false;
	SetPositionImpl(new_position);
}

void OrthographicCamera::Translate(const V3_float& position_change) {
	CreateInstance();
	SetPosition(instance_->position + position_change * GetQuaternion());
}

void OrthographicCamera::Translate(const V2_float& position_change) {
	CreateInstance();
	SetPosition(
		instance_->position +
		V3_float{ position_change.x, position_change.y, 0.0f } * GetQuaternion()
	);
}

void OrthographicCamera::SetZoom(float new_zoom) {
	CreateInstance();
	instance_->zoom = new_zoom;
	instance_->zoom = std::clamp(new_zoom, epsilon<float>, std::numeric_limits<float>::max());
	instance_->recalculate_projection = true;
	RefreshBounds();
}

void OrthographicCamera::Zoom(float zoom_change) {
	CreateInstance();
	SetZoom(instance_->zoom + zoom_change);
}

void OrthographicCamera::SetRotation(const V3_float& new_angle_radians) {
	CreateInstance();
	instance_->orientation		= new_angle_radians;
	instance_->recalculate_view = true;
}

void OrthographicCamera::Rotate(const V3_float& angle_change_radians) {
	CreateInstance();
	SetRotation(instance_->orientation + angle_change_radians);
}

void OrthographicCamera::SetRotation(float yaw_radians) {
	SetYaw(yaw_radians);
}

void OrthographicCamera::Rotate(float yaw_change_radians) {
	Yaw(yaw_change_radians);
}

void OrthographicCamera::SetYaw(float angle_radians) {
	CreateInstance();
	instance_->orientation.x = angle_radians;
}

void OrthographicCamera::SetPitch(float angle_radians) {
	CreateInstance();
	instance_->orientation.y = angle_radians;
}

void OrthographicCamera::SetRoll(float angle_radians) {
	CreateInstance();
	instance_->orientation.z = angle_radians;
}

void OrthographicCamera::Yaw(float angle_change) {
	Rotate({ angle_change, 0.0f, 0.0f });
}

void OrthographicCamera::Pitch(float angle_change) {
	Rotate({ 0.0f, angle_change, 0.0f });
}

void OrthographicCamera::Roll(float angle_change) {
	Rotate({ 0.0f, 0.0f, angle_change });
}

void OrthographicCamera::SetSizeToWindow(bool continuously) {
	if (continuously) {
		instance_->resize_to_window = true;
		SubscribeToWindowResize();
	} else {
		SetSize(game.window.GetSize());
	}
}

void OrthographicCamera::SetSizeImpl(const V2_float& size) {
	PTGN_ASSERT(IsValid());
	instance_->size					  = size;
	instance_->recalculate_projection = true;
	RefreshBounds();
}

void OrthographicCamera::SetSize(const V2_float& size) {
	CreateInstance();
	instance_->resize_to_window = false;
	SetSizeImpl(size);
}

void OrthographicCamera::SetBounds(const Rectangle<float>& bounding_box) {
	CreateInstance();
	instance_->bounding_box = bounding_box;
	// Reset position to ensure it is within the new bounds.
	RefreshBounds();
}

void OrthographicCamera::RecalculateViewProjection() {
	PTGN_ASSERT(IsValid());
	instance_->view_projection = instance_->projection * instance_->view;
}

void OrthographicCamera::RecalculateView() {
	PTGN_ASSERT(IsValid());

	V3_float pos{ -instance_->position.x, -instance_->position.y, instance_->position.z };

	Quaternion orientation = GetQuaternion();
	instance_->view		   = M4_float::Translate(orientation.ToMatrix4(), pos);
}

bool OrthographicCamera::operator==(const OrthographicCamera& o) const {
	return !IsValid() && !o.IsValid() ||
		   (IsValid() && o.IsValid() && instance_->view_projection == o.instance_->view_projection);
}

bool OrthographicCamera::operator!=(const OrthographicCamera& o) const {
	return !(*this == o);
}

void OrthographicCamera::PrintInfo() const {
	auto bounds		 = GetBounds();
	auto orientation = GetOrientation();
	Print(
		"Position: ", GetPosition(), ", Size: ", GetSize(), ", Zoom: ", GetZoom(),
		", Orientation (yaw/pitch/roll) (deg): (", RadToDeg(orientation.x), ", ",
		RadToDeg(orientation.y), ", ", RadToDeg(orientation.z), "), Bounds: "
	);
	if (bounds.IsZero()) {
		PrintLine("none");
	} else {
		PrintLine(bounds.Min(), "->", bounds.Max());
	}
}

void OrthographicCamera::RecalculateProjection() {
	PTGN_ASSERT(instance_->zoom > 0.0f);
	V2_float extents{ instance_->size / 2.0f / instance_->zoom };

	instance_->projection = M4_float::Orthographic(
		-extents.x, extents.x, extents.y, -extents.y, -std::numeric_limits<float>::max(),
		std::numeric_limits<float>::max()
	);
}

/*
// To move camera according to mouse drag (in 3D):
void CameraController::OnMouseMoveEvent([[maybe_unused]] const MouseMoveEvent& e) {

	static bool first_mouse = true;

	if (game.input.MousePressed(Mouse::Left)) {
		const MouseMoveEvent& mouse = static_cast<const MouseMoveEvent&>(e);
		if (!first_mouse) {
			V2_float offset = mouse.current - mouse.previous;

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
	game.event.mouse.Subscribe(MouseEvent::Move, (void*)this, std::function([&](const
MouseMoveEvent& e) { OnMouseMoveEvent(e);
	}));
}

void CameraController::UnsubscribeFromMouseEvents() {
	game.event.mouse.Unsubscribe((void*)this);
}
*/

CameraManager::CameraManager() {
	primary_camera_.SubscribeToWindowResize();
	window_camera_.SubscribeToWindowResize();
}

CameraManager::~CameraManager() {}

void CameraManager::SetPrimary(const Key& key) {
	PTGN_ASSERT(Has(key), "Cannot set camera which has not been loaded as the primary camera");
	primary_camera_ = Get(key);
}

void CameraManager::SetPrimary(const OrthographicCamera& camera) {
	primary_camera_ = camera;
}

const OrthographicCamera& CameraManager::GetCurrent() const {
	if (primary_) {
		return primary_camera_;
	}
	return window_camera_;
}

OrthographicCamera& CameraManager::GetCurrent() {
	PTGN_ASSERT(
		primary_, "Cannot retrieve non-const current camera from camera manager when it is set to "
				  "window camera"
	);
	return primary_camera_;
}

void CameraManager::SetCameraPrimary() {
	primary_ = true;
}

void CameraManager::SetCameraWindow() {
	primary_ = false;
}

void CameraManager::Update() {
	if (primary_ && primary_camera_.IsValid()) {
		game.renderer.UpdateViewProjection(primary_camera_.GetViewProjection());
	} else {
		PTGN_ASSERT(window_camera_.IsValid());
		game.renderer.UpdateViewProjection(window_camera_.GetViewProjection());
	}
}

CameraManager::Item& ActiveSceneCameraManager::LoadImpl(const Key& key, Item&& item) {
	if (!item.IsValid()) {
		item.SetSizeToWindow();
	}
	return game.scene.GetTopActive().camera.Load(key, std::move(item));
}

void ActiveSceneCameraManager::Unload(const Key& key) {
	game.scene.GetTopActive().camera.Unload(key);
}

bool ActiveSceneCameraManager::Has(const Key& key) {
	return game.scene.GetTopActive().camera.Has(key);
}

CameraManager::Item& ActiveSceneCameraManager::Get(const Key& key) {
	return game.scene.GetTopActive().camera.Get(key);
}

void ActiveSceneCameraManager::Clear() {
	game.scene.GetTopActive().camera.Clear();
}

void ActiveSceneCameraManager::SetPrimary(const Key& key) {
	game.scene.GetTopActive().camera.SetPrimary(key);
}

void ActiveSceneCameraManager::SetPrimary(const OrthographicCamera& camera) {
	game.scene.GetTopActive().camera.SetPrimary(camera);
}

const OrthographicCamera& ActiveSceneCameraManager::GetCurrent() const {
	return game.scene.GetTopActive().camera.GetCurrent();
}

OrthographicCamera& ActiveSceneCameraManager::GetCurrent() {
	return game.scene.GetTopActive().camera.GetCurrent();
}

void ActiveSceneCameraManager::SetCameraWindow() {
	game.scene.GetTopActive().camera.SetCameraWindow();
}

void ActiveSceneCameraManager::SetCameraPrimary() {
	game.scene.GetTopActive().camera.SetCameraPrimary();
}

} // namespace ptgn