#include "scene/camera.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <type_traits>

#include "core/manager.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "protegon/events.h"
#include "protegon/game.h"
#include "protegon/log.h"
#include "protegon/math.h"
#include "protegon/matrix4.h"
#include "protegon/polygon.h"
#include "protegon/quaternion.h"
#include "protegon/scene.h"
#include "protegon/vector2.h"
#include "protegon/vector3.h"
#include "renderer/flip.h"
#include "renderer/origin.h"
#include "scene_manager.h"
#include "utility/debug.h"
#include "utility/handle.h"

namespace ptgn {

V2_float WorldToScreen(const V2_float& world_position) {
	const auto& primary{ game.camera.GetPrimary() };
	float scale{ primary.GetZoom() };
	PTGN_ASSERT(scale != 0.0f);
	return (world_position - primary.GetPosition()) * scale + primary.GetSize() / 2.0f;
}

V2_float ScreenToWorld(const V2_float& screen_position) {
	const auto& primary{ game.camera.GetPrimary() };
	float scale{ primary.GetZoom() };
	PTGN_ASSERT(scale != 0.0f);
	return (screen_position - primary.GetSize() / 2.0f) / scale + primary.GetPosition();
}

V2_float ScaleToWorld(const V2_float& screen_size) {
	const auto& primary{ game.camera.GetPrimary() };
	float scale{ primary.GetZoom() };
	PTGN_ASSERT(scale != 0.0f);
	return screen_size / scale;
}

float ScaleToWorld(float screen_size) {
	const auto& primary{ game.camera.GetPrimary() };
	float scale{ primary.GetZoom() };
	PTGN_ASSERT(scale != 0.0f);
	return screen_size / scale;
}

V2_float ScaleToScreen(const V2_float& world_size) {
	const auto& primary{ game.camera.GetPrimary() };
	float scale{ primary.GetZoom() };
	PTGN_ASSERT(scale != 0.0f);
	return world_size * scale;
}

float ScaleToScreen(float world_size) {
	const auto& primary{ game.camera.GetPrimary() };
	float scale{ primary.GetZoom() };
	PTGN_ASSERT(scale != 0.0f);
	return world_size * scale;
}

namespace impl {

Camera::~Camera() {
	if (game.event.window.IsSubscribed(this)) {
		game.event.window.Unsubscribe(this);
	}
}

} // namespace impl

Rectangle<float> OrthographicCamera::GetBounds() const {
	return Get().bounding_box;
}

V2_float OrthographicCamera::GetPosition() const {
	auto& o{ Get() };
	return { o.position.x, o.position.y };
}

V3_float OrthographicCamera::GetPosition3D() const {
	return Get().position;
}

Rectangle<float> OrthographicCamera::GetRectangle() const {
	return Rectangle<float>{ GetTopLeftPosition(), GetSize(), Origin::TopLeft };
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
	if (auto o{ GetPtr() }; !game.event.window.IsSubscribed(o)) {
		game.event.window.Subscribe(
			WindowEvent::Resized, o,
			std::function([this](const WindowResizedEvent& e) { OnWindowResize(e.size); })
		);
	}
	OnWindowResize(game.window.GetSize());
}

void OrthographicCamera::UnsubscribeFromWindowResize() const {
	if (!IsValid()) {
		return;
	}

	if (auto o{ GetPtr() }; game.event.window.IsSubscribed(o)) {
		game.event.window.Unsubscribe(o);
	}
}

const M4_float& OrthographicCamera::GetView() {
	const auto& o{ Get() };
	if (o.recalculate_view) {
		RecalculateView();
	}
	return o.view;
}

const M4_float& OrthographicCamera::GetProjection() {
	const auto& o{ Get() };
	if (o.recalculate_projection) {
		RecalculateProjection();
	}
	return o.projection;
}

const M4_float& OrthographicCamera::GetViewProjection() {
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

void OrthographicCamera::OnWindowResize(const V2_float& size) {
	const auto& o{ Get() };
	if (o.resize_to_window) {
		SetSizeImpl(size);
	}
	if (o.center_to_window) {
		SetPositionImpl({ size.x / 2.0f, size.y / 2.0f, o.position.z });
	}
}

void OrthographicCamera::SetPosition(const V2_float& new_position) {
	Create();
	SetPosition({ new_position.x, new_position.y, Get().position.z });
}

void OrthographicCamera::RefreshBounds() {
	auto& o{ Get() };
	if (!o.bounding_box.IsZero()) {
		V2_float min{ o.bounding_box.Min() };
		V2_float max{ o.bounding_box.Max() };
		PTGN_ASSERT(min.x < max.x && min.y < max.y, "Bounding box min must be below maximum");
		V2_float center{ o.bounding_box.Center() };
		// Draw bounding box center.
		// game.draw.Point(center, color::Red, 5.0f);
		// Draw bounding box.
		// game.draw.RectangleHollow(o.bounding_box, color::Red);

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

void OrthographicCamera::SetBounds(const Rectangle<float>& bounding_box) {
	Create();
	Get().bounding_box = bounding_box;
	// Reset position to ensure it is within the new bounds.
	RefreshBounds();
}

void OrthographicCamera::RecalculateViewProjection() {
	auto& o{ Get() };
	o.view_projection = o.projection * o.view;
}

void OrthographicCamera::RecalculateView() {
	auto& o{ Get() };

	V3_float pos{ -o.position.x, -o.position.y, o.position.z };

	Quaternion orientation = GetQuaternion();
	o.view				   = M4_float::Translate(orientation.ToMatrix4(), pos);
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
	o.projection = M4_float::Orthographic(
		flip.x * -extents.x, flip.x * extents.x, flip.y * extents.y, flip.y * -extents.y,
		-std::numeric_limits<float>::max(), std::numeric_limits<float>::max()
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
	game.event.mouse.Subscribe(MouseEvent::Move, this, std::function([&](const
MouseMoveEvent& e) { OnMouseMoveEvent(e);
	}));
}

void CameraController::UnsubscribeFromMouseEvents() {
	game.event.mouse.Unsubscribe(this);
}
*/

namespace impl {

CameraManager::CameraManager() {
	OrthographicCamera c;
	c.SubscribeToWindowResize();
	primary_cameras_.emplace(0, c);
}

void CameraManager::SetPrimary(const OrthographicCamera& camera, std::size_t render_layer) {
	primary_cameras_[render_layer] = camera;
}

const OrthographicCamera& CameraManager::GetPrimary(std::size_t render_layer) const {
	auto it = primary_cameras_.find(render_layer);
	PTGN_ASSERT(
		it != primary_cameras_.end(), "Primary camera does not exist for the given render layer"
	);
	return it->second;
}

OrthographicCamera& CameraManager::GetPrimary(std::size_t render_layer) {
	if (auto it = primary_cameras_.find(render_layer); it != primary_cameras_.end()) {
		return it->second;
	}
	OrthographicCamera c;
	c.SubscribeToWindowResize();
	auto new_it = primary_cameras_.emplace(render_layer, c);
	return new_it.first->second;
}

void CameraManager::SetPrimaryImpl(const InternalKey& key, std::size_t render_layer) {
	PTGN_ASSERT(Has(key), "Cannot set camera which has not been loaded as the primary camera");
	SetPrimary(Get(key), render_layer);
}

void CameraManager::Reset() {
	MapManager::Reset();
	ResetPrimary();
}

void CameraManager::ResetPrimary() {
	for (auto& [layer, camera] : primary_cameras_) {
		camera.UnsubscribeFromWindowResize();
		camera = {};
		camera.SubscribeToWindowResize();
	}
}

CameraManager::Item& ActiveSceneCameraManager::LoadImpl(const InternalKey& key, Item&& item) {
	if (!item.IsValid()) {
		item.SetSizeToWindow();
	}
	return game.scene.GetTopActive().camera.Load(key, std::move(item));
}

void ActiveSceneCameraManager::UnloadImpl(const InternalKey& key) {
	game.scene.GetTopActive().camera.Unload(key);
}

bool ActiveSceneCameraManager::HasImpl(const InternalKey& key) {
	return game.scene.GetTopActive().camera.Has(key);
}

CameraManager::Item& ActiveSceneCameraManager::GetImpl(const InternalKey& key) {
	return game.scene.GetTopActive().camera.Get(key);
}

void ActiveSceneCameraManager::Clear() {
	game.scene.GetTopActive().camera.Clear();
}

void ActiveSceneCameraManager::SetPrimaryImpl(const InternalKey& key, std::size_t render_layer) {
	game.scene.GetTopActive().camera.SetPrimary(key, render_layer);
}

void ActiveSceneCameraManager::SetPrimary(
	const OrthographicCamera& camera, std::size_t render_layer
) {
	game.scene.GetTopActive().camera.SetPrimary(camera, render_layer);
}

const OrthographicCamera& ActiveSceneCameraManager::GetPrimary(std::size_t render_layer) const {
	return game.scene.GetTopActive().camera.GetPrimary(render_layer);
}

OrthographicCamera& ActiveSceneCameraManager::GetPrimary(std::size_t render_layer) {
	return game.scene.GetTopActive().camera.GetPrimary(render_layer);
}

void ActiveSceneCameraManager::Reset() {
	game.scene.GetTopActive().camera.Reset();
}

void ActiveSceneCameraManager::ResetPrimary() {
	game.scene.GetTopActive().camera.ResetPrimary();
}

} // namespace impl

} // namespace ptgn