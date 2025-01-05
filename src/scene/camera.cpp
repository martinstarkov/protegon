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
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/log.h"

namespace ptgn {

namespace impl {

Camera::~Camera() {
	game.event.window.Unsubscribe(this);
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

} // namespace impl

Rect OrthographicCamera::GetBounds() const {
	return Get().bounding_box;
}

V2_float OrthographicCamera::GetPosition() const {
	auto& o{ Get() };
	return { o.position.x, o.position.y };
}

V3_float OrthographicCamera::GetPosition3D() const {
	return Get().position;
}

void OrthographicCamera::SetToWindow(bool continuously) {
	if (continuously) {
		UnsubscribeFromWindowResize();
	}
	if (IsValid()) {
		Get().Reset();
	}
	CenterOnWindow(continuously);
	SetSizeToWindow(continuously);
}

void OrthographicCamera::CenterOnArea(const V2_float& size) {
	SetSize(size);
	SetPosition(size / 2.0f);
}

Rect OrthographicCamera::GetRect() const {
	return Rect{ GetTopLeftPosition(), GetSize(), Origin::TopLeft };
}

V2_float OrthographicCamera::GetTopLeftPosition() const {
	return GetPosition() - GetSize() / 2.0f;
}

V2_float OrthographicCamera::GetSize() const {
	return Get().size;
}

float OrthographicCamera::GetZoom() const {
	return Get().zoom;
}

V3_float OrthographicCamera::GetOrientation() const {
	return Get().orientation;
}

Quaternion OrthographicCamera::GetQuaternion() const {
	return Quaternion::FromEuler(Get().orientation);
}

Flip OrthographicCamera::GetFlip() const {
	return Get().flip;
}

void OrthographicCamera::SetFlip(Flip flip) {
	Create();
	Get().flip = flip;
}

void OrthographicCamera::CenterOnWindow(bool continuously) {
	Create();
	if (continuously) {
		Get().center_to_window = true;
		SubscribeToWindowResize();
	} else {
		V2_float s = game.window.GetSize();
		SetPositionImpl({ s.x / 2.0f, s.y / 2.0f, Get().position.z });
	}
}

void OrthographicCamera::SubscribeToWindowResize() {
	Create();
	auto window_resize = std::function([*this](const WindowResizedEvent& e) mutable {
		const auto& camera = Get();
		if (camera.resize_to_window) {
			SetSizeImpl(e.size);
		}
		if (camera.center_to_window) {
			SetPositionImpl({ static_cast<float>(e.size.x) / 2.0f,
							  static_cast<float>(e.size.y) / 2.0f, GetPosition3D().z });
		}
	});
	std::invoke(window_resize, WindowResizedEvent{ game.window.GetSize() });
	if (!game.event.window.IsSubscribed(&Get())) {
		game.event.window.Subscribe(
			WindowEvent::Resized, &Get(), std::move(window_resize)

		);
	}
}

void OrthographicCamera::UnsubscribeFromWindowResize() const {
	if (!IsValid()) {
		return;
	}

	game.event.window.Unsubscribe(&Get());
}

const Matrix4& OrthographicCamera::GetView() const {
	const auto& o{ Get() };
	if (o.recalculate_view) {
		RecalculateView();
	}
	return o.view;
}

const Matrix4& OrthographicCamera::GetProjection() const {
	const auto& o{ Get() };
	if (o.recalculate_projection) {
		RecalculateProjection();
	}
	return o.projection;
}

const Matrix4& OrthographicCamera::GetViewProjection() const {
	auto& o{ Get() };
	bool updated_matrix{ o.recalculate_view || o.recalculate_projection };
	if (o.recalculate_view) {
		RecalculateView();
		o.recalculate_view = false;
	}
	if (o.recalculate_projection) {
		RecalculateProjection();
		o.recalculate_projection = false;
	}
	if (updated_matrix) {
		RecalculateViewProjection();
	}
	return o.view_projection;
}

void OrthographicCamera::SetPosition(const V2_float& new_position) {
	Create();
	SetPosition({ new_position.x, new_position.y, Get().position.z });
}

void OrthographicCamera::RefreshBounds() {
	auto& o{ Get() };
	if (o.bounding_box.IsZero()) {
		return;
	}
	V2_float min{ o.bounding_box.Min() };
	V2_float max{ o.bounding_box.Max() };
	PTGN_ASSERT(min.x < max.x && min.y < max.y, "Bounding box min must be below maximum");
	V2_float center{ o.bounding_box.Center() };
	// Draw bounding box center.
	// game.draw.Point(center, color::Red, 5.0f);
	// Draw bounding box.
	// game.draw.RectHollow(o.bounding_box, color::Red);

	// TODO: Incoporate yaw, i.e. o.orientation.x into the bounds using sin and cos.
	V2_float size{ o.size / o.zoom };
	V2_float half{ size * 0.5f };
	if (size.x > o.bounding_box.size.x) {
		o.position.x = center.x;
	} else {
		o.position.x = std::clamp(o.position.x, min.x + half.x, max.x - half.x);
	}
	if (size.y > o.bounding_box.size.y) {
		o.position.y = center.y;
	} else {
		o.position.y = std::clamp(o.position.y, min.y + half.y, max.y - half.y);
	}
	// Draw clamped camera position.
	/*game.draw.Point(
		{ o.position.x, o.position.y }, color::Yellow, 5.0f
	);*/
	o.recalculate_view = true;
}

void OrthographicCamera::SetPositionImpl(const V3_float& new_position) {
	auto& o{ Get() };
	o.position		   = new_position;
	o.recalculate_view = true;
	RefreshBounds();
}

void OrthographicCamera::SetPosition(const V3_float& new_position) {
	Create();
	Get().center_to_window = false;
	SetPositionImpl(new_position);
}

void OrthographicCamera::Translate(const V3_float& position_change) {
	Create();
	SetPosition(Get().position + position_change * GetQuaternion());
}

void OrthographicCamera::Translate(const V2_float& position_change) {
	Create();
	SetPosition(
		Get().position + V3_float{ position_change.x, position_change.y, 0.0f } * GetQuaternion()
	);
}

void OrthographicCamera::SetZoom(float new_zoom) {
	Create();
	auto& o{ Get() };
	o.zoom = new_zoom;
	o.zoom = std::clamp(new_zoom, epsilon<float>, std::numeric_limits<float>::max());
	o.recalculate_projection = true;
	RefreshBounds();
}

void OrthographicCamera::Zoom(float zoom_change) {
	Create();
	SetZoom(Get().zoom + zoom_change);
}

void OrthographicCamera::SetRotation(const V3_float& new_angle_radians) {
	Create();
	auto& o{ Get() };
	o.orientation	   = new_angle_radians;
	o.recalculate_view = true;
}

void OrthographicCamera::Rotate(const V3_float& angle_change_radians) {
	Create();
	SetRotation(Get().orientation + angle_change_radians);
}

void OrthographicCamera::SetRotation(float yaw_radians) {
	SetYaw(yaw_radians);
}

void OrthographicCamera::Rotate(float yaw_change_radians) {
	Yaw(yaw_change_radians);
}

void OrthographicCamera::SetYaw(float angle_radians) {
	Create();
	Get().orientation.x = angle_radians;
}

void OrthographicCamera::SetPitch(float angle_radians) {
	Create();
	Get().orientation.y = angle_radians;
}

void OrthographicCamera::SetRoll(float angle_radians) {
	Create();
	Get().orientation.z = angle_radians;
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
	Create();
	if (continuously) {
		Get().resize_to_window = true;
		SubscribeToWindowResize();
	} else {
		SetSize(game.window.GetSize());
	}
}

void OrthographicCamera::SetSizeImpl(const V2_float& size) {
	auto& o{ Get() };
	o.size					 = size;
	o.recalculate_projection = true;
	RefreshBounds();
}

void OrthographicCamera::SetSize(const V2_float& size) {
	Create();
	Get().resize_to_window = false;
	SetSizeImpl(size);
}

void OrthographicCamera::SetBounds(const Rect& bounding_box) {
	Create();
	Get().bounding_box = bounding_box;
	// Reset position to ensure it is within the new bounds.
	RefreshBounds();
}

void OrthographicCamera::RecalculateViewProjection() const {
	auto& o{ Get() };
	o.view_projection = o.projection * o.view;
}

void OrthographicCamera::RecalculateView() const {
	auto& o{ Get() };

	V3_float position{ -o.position.x, -o.position.y, o.position.z };

	Quaternion orientation = GetQuaternion();
	o.view				   = Matrix4::Translate(orientation.ToMatrix4(), position);
}

void OrthographicCamera::RecalculateProjection() const {
	auto& o{ Get() };
	PTGN_ASSERT(o.zoom > 0.0f);
	V2_float extents{ o.size / 2.0f / o.zoom };
	V2_float flip{ 1.0f, 1.0f };
	switch (o.flip) {
		case Flip::None:	   break;
		case Flip::Vertical:   flip.y = -1.0f; break;
		case Flip::Horizontal: flip.x = -1.0f; break;
		case Flip::Both:
			flip.x = -1.0f;
			flip.y = -1.0f;
			break;
		default: PTGN_ERROR("Unrecognized flip state");
	}
	o.projection = Matrix4::Orthographic(
		flip.x * -extents.x, flip.x * extents.x, flip.y * extents.y, flip.y * -extents.y,
		-std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()
	);
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

CameraManager::CameraManager() {
	ResetPrimary();
}

void CameraManager::SetPrimary(const OrthographicCamera& camera) {
	primary_camera_ = camera;
}

OrthographicCamera CameraManager::GetPrimary() const {
	return primary_camera_;
}

void CameraManager::SetPrimaryImpl(const InternalKey& key) {
	PTGN_ASSERT(Has(key), "Cannot set camera which has not been loaded as the primary camera");
	SetPrimary(Get(key));
}

void CameraManager::Reset() {
	MapManager::Reset();
	ResetPrimary();
}

void CameraManager::ResetPrimary() {
	primary_camera_.SetToWindow();
}

const OrthographicCamera& CameraManager::GetWindow() {
	return game.camera.GetWindow();
}

namespace impl {

CameraManager::Item& SceneCamera::LoadImpl(const InternalKey& key, Item&& item) {
	if (!item.IsValid()) {
		item.SetSizeToWindow();
	}
	if (game.scene.HasCurrent()) {
		return game.scene.GetCurrent().GetRenderTarget().GetCamera().Load(key, std::move(item));
	}
	return game.renderer.screen_target_.GetCamera().Load(key, std::move(item));
}

void SceneCamera::UnloadImpl(const InternalKey& key) {
	if (game.scene.HasCurrent()) {
		game.scene.GetCurrent().GetRenderTarget().GetCamera().Unload(key);
	} else {
		game.renderer.screen_target_.GetCamera().Unload(key);
	}
}

bool SceneCamera::HasImpl(const InternalKey& key) {
	if (game.scene.HasCurrent()) {
		return game.scene.GetCurrent().GetRenderTarget().GetCamera().Has(key);
	}
	return game.renderer.screen_target_.GetCamera().Has(key);
}

CameraManager::Item& SceneCamera::GetImpl(const InternalKey& key) {
	if (game.scene.HasCurrent()) {
		return game.scene.GetCurrent().GetRenderTarget().GetCamera().Get(key);
	}
	return game.renderer.screen_target_.GetCamera().Get(key);
}

void SceneCamera::Clear() {
	if (game.scene.HasCurrent()) {
		game.scene.GetCurrent().GetRenderTarget().GetCamera().Clear();
	} else {
		game.renderer.screen_target_.GetCamera().Clear();
	}
}

void SceneCamera::SetPrimaryImpl(const InternalKey& key) {
	if (game.scene.HasCurrent()) {
		game.scene.GetCurrent().GetRenderTarget().GetCamera().SetPrimary(key);
	} else {
		game.renderer.screen_target_.GetCamera().SetPrimary(key);
	}
}

void SceneCamera::Init() {
	window_camera_.SetToWindow(true);
}

const OrthographicCamera& SceneCamera::GetWindow() const {
	return window_camera_;
}

void SceneCamera::SetPrimary(const OrthographicCamera& camera) {
	if (game.scene.HasCurrent()) {
		game.scene.GetCurrent().GetRenderTarget().GetCamera().SetPrimary(camera);
	} else {
		game.renderer.screen_target_.GetCamera().SetPrimary(camera);
	}
}

OrthographicCamera SceneCamera::GetPrimary() {
	if (game.scene.HasCurrent()) {
		return game.scene.GetCurrent().GetRenderTarget().GetCamera().GetPrimary();
	}
	return game.renderer.screen_target_.GetCamera().GetPrimary();
}

void SceneCamera::Reset() {
	if (game.scene.HasCurrent()) {
		game.scene.GetCurrent().GetRenderTarget().GetCamera().Reset();
	} else {
		game.renderer.screen_target_.GetCamera().Reset();
	}
}

void SceneCamera::ResetPrimary() {
	if (game.scene.HasCurrent()) {
		game.scene.GetCurrent().GetRenderTarget().GetCamera().ResetPrimary();
	} else {
		game.renderer.screen_target_.GetCamera().ResetPrimary();
	}
}

} // namespace impl

} // namespace ptgn