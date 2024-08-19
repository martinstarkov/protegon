#include "camera.h"

#include "core/window.h"
#include "protegon/game.h"

namespace ptgn {

const V3_float& OrthographicCamera::GetPosition() const {
	PTGN_ASSERT(IsValid());
	return instance_->position;
}

V2_float OrthographicCamera::GetTopLeftPosition() const {
	return V2_float{ instance_->position.x, instance_->position.y } - instance_->size / 2.0f;
}

const V2_float& OrthographicCamera::GetSize() const {
	PTGN_ASSERT(IsValid());
	return instance_->size;
}

const Quaternion& OrthographicCamera::GetOrientation() const {
	PTGN_ASSERT(IsValid());
	return instance_->orientation;
}

V3_float OrthographicCamera::GetEulerOrientation() const {
	PTGN_ASSERT(IsValid());
	return { instance_->orientation.GetYaw(), instance_->orientation.GetPitch(),
			 instance_->orientation.GetRoll() };
}

const M4_float& OrthographicCamera::GetView() const {
	PTGN_ASSERT(IsValid());
	return instance_->view;
}

const M4_float& OrthographicCamera::GetProjection() const {
	PTGN_ASSERT(IsValid());
	return instance_->projection;
}

const M4_float& OrthographicCamera::GetViewProjection() const {
	PTGN_ASSERT(IsValid());
	return instance_->view_projection;
}

void OrthographicCamera::SetPosition(const V2_float& new_position) {
	SetPosition({ new_position.x, new_position.y, 0.0f });
}

void OrthographicCamera::SetPosition(const V3_float& new_position) {
	PTGN_ASSERT(IsValid());
	// TODO: Add model matrix here to offset by half the projection (screen).
	instance_->position = new_position;
	// Draw camera position before clamping.
	// game.renderer.DrawPoint({ instance_->position.x, instance_->position.y }, color::Blue, 5.0f);
	// Zero bounding box means no bounding box is enforced.
	if (!instance_->bounding_box.IsZero()) {
		V2_float min{ instance_->bounding_box.Min() };
		V2_float max{ instance_->bounding_box.Max() };
		PTGN_ASSERT(min.x < max.x && min.y < max.y, "Bounding box min must be below maximum");
		V2_float center{ instance_->bounding_box.Center() };
		// Draw bounding box center.
		// game.renderer.DrawPoint(center, color::Red, 5.0f);
		// Draw bounding box.
		// game.renderer.DrawRectangleHollow(instance_->bounding_box, color::Red);
		V2_float half{ instance_->size * 0.5f };
		if (instance_->size.x > instance_->bounding_box.size.x) {
			instance_->position.x = center.x;
		} else {
			instance_->position.x =
				std::clamp(instance_->position.x, min.x + half.x, max.x - half.x);
		}
		if (instance_->size.y > instance_->bounding_box.size.y) {
			instance_->position.y = center.y;
		} else {
			instance_->position.y =
				std::clamp(instance_->position.y, min.y + half.y, max.y - half.y);
		}
		// Draw clamped camera position.
		/*game.renderer.DrawPoint(
			{ instance_->position.x, instance_->position.y }, color::Yellow, 5.0f
		);*/
	}
	RecalculateView();
}

void OrthographicCamera::SetRotation(const V3_float& new_angles) {
	PTGN_ASSERT(IsValid());
	instance_->orientation = {};
	// TODO: Check that this works as intended (may need to flip x and y components).
	instance_->orientation *= Quaternion::GetAngleAxis(
		new_angles.x, V3_float{ 0.0f, 1.0f, 0.0f } * instance_->orientation
	);
	instance_->orientation *= Quaternion::GetAngleAxis(
		new_angles.y, V3_float{ 1.0f, 0.0f, 0.0f } * instance_->orientation
	);
	instance_->orientation *= Quaternion::GetAngleAxis(
		new_angles.z, V3_float{ 0.0f, 0.0f, 1.0f } * instance_->orientation
	);
	RecalculateView();
}

void OrthographicCamera::Translate(const V3_float& v) {
	PTGN_ASSERT(IsValid());
	instance_->position += v * instance_->orientation;
	RecalculateView();
}

void OrthographicCamera::Rotate(float angle, const V3_float& axis) {
	PTGN_ASSERT(IsValid());
	instance_->orientation *= Quaternion::GetAngleAxis(angle, axis * instance_->orientation);
	RecalculateView();
}

void OrthographicCamera::Yaw(float angle) {
	Rotate(angle, { 0.0f, 1.0f, 0.0f });
}

void OrthographicCamera::Pitch(float angle) {
	Rotate(angle, { 1.0f, 0.0f, 0.0f });
}

void OrthographicCamera::Roll(float angle) {
	Rotate(angle, { 0.0f, 0.0f, 1.0f });
}

void OrthographicCamera::RecalculateViewProjection() {
	PTGN_ASSERT(IsValid());
	instance_->view_projection = instance_->projection * instance_->view;
}

void OrthographicCamera::RecalculateView() {
	PTGN_ASSERT(IsValid());
	V2_float ws{ game.window.GetSize() };

	// TODO: Instead of subtracting half the window size, incorporate this into the projection
	// matrix.
	instance_->view = M4_float::Translate(
		instance_->orientation.ToMatrix4(),
		V3_float{ -instance_->position.x, -instance_->position.y, instance_->position.z } +
			V3_float{ instance_->size.x * 0.5f, instance_->size.y * 0.5f, 0.0f }
	);

	RecalculateViewProjection();
}

bool OrthographicCamera::operator==(const OrthographicCamera& o) const {
	return !IsValid() && !o.IsValid() ||
		   (IsValid() && o.IsValid() && instance_->view_projection == o.instance_->view_projection);
}

bool OrthographicCamera::operator!=(const OrthographicCamera& o) const {
	return !(*this == o);
}

OrthographicCamera::OrthographicCamera() {
	game.event.window.Subscribe(
		WindowEvent::Resized, (void*)this,
		std::function([&](const WindowResizedEvent& e) { SetSize(e.size); })
	);
}

OrthographicCamera::~OrthographicCamera() {
	game.event.window.Unsubscribe((void*)this);
}

OrthographicCamera::OrthographicCamera(
	float left, float right, float bottom, float top, float near /* = -1.0f*/, float far /* = 1.0f*/
) {
	if (!IsValid()) {
		instance_ = std::make_shared<Camera>();
	}
	SetProjection(left, right, bottom, top, near, far);
}

void OrthographicCamera::SetSizeToWindow() {
	SetSize(game.window.GetSize());
}

void OrthographicCamera::SetSize(const V2_float& size) {
	// TODO: Incorporate camera centering here?
	SetProjection(0.0f, size.x, size.y, 0.0f);
}

void OrthographicCamera::SetProjection(
	float left, float right, float bottom, float top, float near /* = -1.0f*/, float far /*= 1.0f*/
) {
	if (!IsValid()) {
		instance_ = std::make_shared<Camera>();
	}
	PTGN_ASSERT(IsValid(), "Cannot set projection matrix of uninitialized or destroyed camera");
	instance_->size		  = { right - left, bottom - top };
	instance_->projection = M4_float::Orthographic(left, right, bottom, top, near, far);
	RecalculateViewProjection();
}

void OrthographicCamera::SetClampBounds(const Rectangle<float>& bounding_box) {
	PTGN_ASSERT(IsValid(), "Cannot set clamp bounds of uninitialized or destroyed camera");
	instance_->bounding_box = bounding_box;
	// Reset position to ensure it is within the new bounds.
	SetPosition(instance_->position);
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
	OnWindowResize(game.window.GetSize());
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
	window_camera_.SetSizeToWindow();
	if (reset_primary) {
		ResetPrimaryToWindow();
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

OrthographicCamera& ActiveSceneCameraManager::GetPrimary() {
	return game.scene.GetTopActive().camera.GetPrimary();
}

void ActiveSceneCameraManager::ResetPrimaryToWindow() {
	game.scene.GetTopActive().camera.ResetPrimaryToWindow();
}

} // namespace ptgn
