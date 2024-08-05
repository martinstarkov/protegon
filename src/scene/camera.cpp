#include "camera.h"

#include "core/window.h"
#include "protegon/game.h"

namespace ptgn {

const V3_float& Camera::GetPosition() const {
	return position_;
}

const Quaternion& Camera::GetOrientation() const {
	return orientation_;
}

V3_float Camera::GetEulerOrientation() const {
	return { orientation_.GetYaw(), orientation_.GetPitch(), orientation_.GetRoll() };
}

const M4_float& Camera::GetView() const {
	return view_;
}

const M4_float& Camera::GetProjection() const {
	return projection_;
}

const M4_float& Camera::GetViewProjection() const {
	return view_projection_;
}

void Camera::SetPosition(const V2_float& new_position) {
	SetPosition({ new_position.x, new_position.y, 0.0f });
}

void Camera::SetPosition(const V3_float& new_position) {
	// TODO: Add model matrix here to offset by half the projection (screen).
	position_ = new_position;
	RecalculateView();
}

void Camera::SetRotation(const V3_float& new_angles) {
	orientation_ = {};
	// TODO: Check that this works as intended (may need to flip x and y components).
	orientation_ *=
			Quaternion::GetAngleAxis(new_angles.x, V3_float{ 0.0f, 1.0f, 0.0f } * orientation_);
	orientation_ *=
			Quaternion::GetAngleAxis(new_angles.y, V3_float{ 1.0f, 0.0f, 0.0f } * orientation_);
	orientation_ *=
			Quaternion::GetAngleAxis(new_angles.z, V3_float{ 0.0f, 0.0f, 1.0f } * orientation_);
	RecalculateView();
}

void Camera::Translate(const V3_float& v) {
	position_ += v * orientation_;
	RecalculateView();
}

void Camera::Rotate(float angle, const V3_float& axis) {
	orientation_ *= Quaternion::GetAngleAxis(angle, axis * orientation_);
	RecalculateView();
}

void Camera::Yaw(float angle) {
	Rotate(angle, { 0.0f, 1.0f, 0.0f });
}

void Camera::Pitch(float angle) {
	Rotate(angle, { 1.0f, 0.0f, 0.0f });
}

void Camera::Roll(float angle) {
	Rotate(angle, { 0.0f, 0.0f, 1.0f });
}

void Camera::RecalculateViewProjection() {
	view_projection_ = projection_ * view_;
}

void Camera::RecalculateView() {
	view_ = M4_float::Translate(orientation_.ToMatrix4(), position_);
	RecalculateViewProjection();
}

bool Camera::operator==(const Camera& o) const {
	return view_projection_ == o.view_projection_;
}

bool Camera::operator!=(const Camera& o) const {
	return !(*this == o);
}

OrthographicCamera::OrthographicCamera() {
	V2_float ws{ game.window.GetSize() };
	SetProjection(0.0f, ws.x, ws.y, 0.0f);

	game.event.window.Subscribe(
			WindowEvent::Resized, (void*)this, std::function([&](const WindowResizedEvent& e) {
				SetProjection(
						0.0f, static_cast<float>(e.size.x), static_cast<float>(e.size.y), 0.0f
				);
			})
	);
}

OrthographicCamera::~OrthographicCamera() {
	game.event.window.Unsubscribe((void*)this);
}

OrthographicCamera::OrthographicCamera(
		float left, float right, float bottom, float top, float near /* = -1.0f*/,
		float far /* = 1.0f*/
) {
	SetProjection(left, right, bottom, top, near, far);
}

void OrthographicCamera::SetProjection(
		float left, float right, float bottom, float top, float near /* = -1.0f*/,
		float far /*= 1.0f*/
) {
	projection_ = M4_float::Orthographic(left, right, bottom, top, near, far);
	RecalculateViewProjection();
}

/*
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
	game.event.window.Subscribe(
			WindowEvent::Resized, (void*)this,
			std::function([&](const WindowResizedEvent& e) { OnWindowResize(e.size); })
	);
	ResetPrimaryToWindow();
}

CameraManager::~CameraManager() {
	game.event.window.Unsubscribe((void*)this);
}

void CameraManager::SetPrimary(const Key& key) {
	PTGN_ASSERT(Has(key), "Cannot set camera which has not been loaded as the primary camera");
	primary_camera_ = Get(key);
	game.renderer.UpdateViewProjection(primary_camera_.GetViewProjection());
}

const OrthographicCamera& CameraManager::GetPrimary() const {
	return primary_camera_;
}

OrthographicCamera& CameraManager::GetPrimary() {
	return primary_camera_;
}

void CameraManager::ResetPrimaryToWindow() {
	primary_camera_ = window_camera_;
	Update();
}

void CameraManager::Update() {
	static M4_float prev_vp = primary_camera_.GetViewProjection();
	const M4_float& vp		= primary_camera_.GetViewProjection();
	if (vp != prev_vp) {
		prev_vp = vp;
		game.renderer.UpdateViewProjection(vp);
	}
}

void CameraManager::OnWindowResize(const V2_float& size) {
	bool reset_primary{ primary_camera_ == window_camera_ };
	window_camera_ = {};
	if (reset_primary) {
		ResetPrimaryToWindow();
	}
}

CameraManager::Item& ActiveSceneCameraManager::LoadImpl(const Key& key, Item&& item) {
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

OrthographicCamera& ActiveSceneCameraManager::GetPrimary() {
	return game.scene.GetTopActive().camera.GetPrimary();
}

void ActiveSceneCameraManager::ResetPrimaryToWindow() {
	game.scene.GetTopActive().camera.ResetPrimaryToWindow();
}

} // namespace ptgn
