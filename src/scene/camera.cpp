#include "scene/camera.h"

#include <algorithm>
#include <array>
#include <limits>

#include "common/assert.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/script.h"
#include "core/time.h"
#include "core/window.h"
#include "debug/log.h"
#include "math/easing.h"
#include "math/geometry.h"
#include "math/geometry/rect.h"
#include "math/math.h"
#include "math/matrix4.h"
#include "math/quaternion.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "renderer/api/flip.h"
#include "renderer/api/origin.h"
#include "renderer/render_data.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "scene/scene_key.h"
#include "scene/scene_manager.h"
#include "tweens/follow_config.h"
#include "tweens/shake_config.h"
#include "tweens/tween_effects.h"

namespace ptgn {

namespace impl {

void CameraInfo::SetViewportPosition(const V2_float& new_viewport_position) {
	if (viewport_position == new_viewport_position) {
		return;
	}
	viewport_position = new_viewport_position;
	projection_dirty  = true;
}

void CameraInfo::SetViewportSize(const V2_float& new_viewport_size) {
	if (viewport_size == new_viewport_size) {
		return;
	}
	viewport_size	 = new_viewport_size;
	projection_dirty = true;
}

void CameraInfo::SetBoundingBox(
	const V2_float& new_bounding_position, const V2_float& new_bounding_size
) {
	if (bounding_box_position == new_bounding_position && bounding_box_size == new_bounding_size) {
		return;
	}
	bounding_box_position = new_bounding_position;
	bounding_box_size	  = new_bounding_size;
	view_dirty			  = true;
}

void CameraInfo::SetResizeToWindow(bool resize) {
	if (resize_to_window == resize) {
		return;
	}
	resize_to_window = resize;
	projection_dirty = true;
}

void CameraInfo::SetCenterOnWindow(bool center) {
	if (center_on_window == center) {
		return;
	}
	center_on_window = center;
	view_dirty		 = true;
}

void CameraInfo::SetFlip(Flip new_flip) {
	if (flip == new_flip) {
		return;
	}
	flip			 = new_flip;
	projection_dirty = true;
}

void CameraInfo::SetPositionZ(float z) {
	if (NearlyEqual(position_z, z)) {
		return;
	}
	position_z = z;
	view_dirty = true;
}

void CameraInfo::SetRotationY(float y_rotation) {
	if (NearlyEqual(orientation_y, y_rotation)) {
		return;
	}
	orientation_y = y_rotation;
	view_dirty	  = true;
}

void CameraInfo::SetRotationZ(float z_rotation) {
	if (NearlyEqual(orientation_z, z_rotation)) {
		return;
	}
	orientation_z = z_rotation;
	view_dirty	  = true;
}

void CameraInfo::SetPixelRounding(bool enabled) {
	if (pixel_rounding == enabled) {
		return;
	}
	pixel_rounding	 = enabled;
	view_dirty		 = true;
	projection_dirty = true;
}

V2_float CameraInfo::GetViewportPosition() const {
	return viewport_position;
}

V2_float CameraInfo::GetViewportSize() const {
	return viewport_size;
}

V2_float CameraInfo::GetBoundingBoxPosition() const {
	return bounding_box_position;
}

V2_float CameraInfo::GetBoundingBoxSize() const {
	return bounding_box_size;
}

bool CameraInfo::GetResizeToWindow() const {
	return resize_to_window;
}

bool CameraInfo::GetCenterOnWindow() const {
	return center_on_window;
}

Flip CameraInfo::GetFlip() const {
	return flip;
}

float CameraInfo::GetPositionZ() const {
	return position_z;
}

float CameraInfo::GetRotationY() const {
	return orientation_y;
}

float CameraInfo::GetRotationZ() const {
	return orientation_z;
}

bool CameraInfo::GetPixelRounding() const {
	return pixel_rounding;
}

const Matrix4& CameraInfo::GetView(const Transform& current, const Entity& entity) const {
	if (view_dirty) {
		RecalculateView(current, GetOffset(entity));
	}
	return view;
}

const Matrix4& CameraInfo::GetProjection(const Transform& current) const {
	if (projection_dirty) {
		RecalculateProjection(current);
	}
	return projection;
}

const Matrix4& CameraInfo::GetViewProjection(const Transform& current, const Entity& entity) const {
	auto offset_transform{ GetOffset(entity) };

	// Either view is dirty, the camera has been offset (due to shake or other effects), or the
	// current position of the camera differs from its previous position (for instance, as a result
	// of a system changing the position of the camera entity externally).
	bool update_view{ view_dirty || current.GetPosition() != previous.GetPosition() ||
					  offset_transform != Transform{} ||
					  !NearlyEqual(current.GetRotation(), previous.GetRotation()) };

	bool update_projection{ projection_dirty || current.GetScale() != previous.GetScale() };

	if (update_view) {
		RecalculateView(current, offset_transform);
	}

	if (update_projection) {
		RecalculateProjection(current);
	}

	if (update_view || update_projection) {
		RecalculateViewProjection();
	}

	previous = current;

	return view_projection;
}

void CameraInfo::RecalculateViewProjection() const {
	view_projection = projection * view;
}

void CameraInfo::RecalculateView(const Transform& current, const Transform& offset_transform)
	const {
	V3_float position{ current.GetPosition().x, current.GetPosition().y, position_z };
	V3_float orientation{ current.GetRotation(), orientation_y, orientation_z };

	position.x	  += offset_transform.GetPosition().x;
	position.y	  += offset_transform.GetPosition().y;
	orientation.x += offset_transform.GetRotation();

	if (!offset_transform.GetPosition().IsZero()) {
		auto zoom{ Abs(current.GetScale()) };
		// Reclamp offset position to ensure camera shake does not move the camera out of
		// bounds.
		auto clamped{ ClampToBounds(
			{ position.x, position.y }, bounding_box_position, bounding_box_size, viewport_size,
			zoom
		) };

		position.x = clamped.x;
		position.y = clamped.y;
	}

	if (pixel_rounding) {
		position = Round(position);
	}

	// Mirrored because camera transforms are in world space and the world must be shown relative to
	// the camera.
	V3_float mirror_position{ -position.x, -position.y, position.z };
	V3_float mirror_orienation{ -orientation.x, -orientation.y, -orientation.z };

	Quaternion quat_orientation{ Quaternion::FromEuler(mirror_orienation) };
	view	   = Matrix4::Translate(quat_orientation.ToMatrix4(), mirror_position);
	view_dirty = false;
}

void CameraInfo::RecalculateProjection(const Transform& current) const {
	auto zoom{ Abs(current.GetScale()) };
	PTGN_ASSERT(zoom.x > 0.0f && zoom.y > 0.0f);
	V2_float extents{ (viewport_size * 0.5f) / zoom };
	if (pixel_rounding) {
		extents = Round(extents);
	}
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
	projection_dirty = false;
}

V2_float CameraInfo::ClampToBounds(
	V2_float position, const V2_float& bounding_box_position, const V2_float& bounding_box_size,
	const V2_float& viewport_size, const V2_float& camera_zoom
) {
	if (bounding_box_size.IsZero()) {
		return position;
	}

	V2_float min{ bounding_box_position };
	V2_float max{ bounding_box_position + bounding_box_size };
	PTGN_ASSERT(min.x < max.x && min.y < max.y, "Bounding box min must be below maximum");

	V2_float center{ Midpoint(min, max) };

	// TODO: Incoporate yaw, i.e. data.orientation.x into the bounds using sin and cos.
	V2_float real_size{ viewport_size / camera_zoom };
	V2_float half{ real_size * 0.5f };

	if (real_size.x > bounding_box_size.x) {
		position.x = center.x;
	} else {
		position.x = std::clamp(position.x, min.x + half.x, max.x - half.x);
	}
	if (real_size.y > bounding_box_size.y) {
		position.y = center.y;
	} else {
		position.y = std::clamp(position.y, min.y + half.y, max.y - half.y);
	}

	return position;
}

void CameraInfo::SetViewDirty() {
	view_dirty = true;
}

Camera CreateCamera(const Entity& entity) {
	Camera camera{ entity };
	SetPosition(camera, {});
	camera.Add<impl::CameraInfo>();
	// TODO: Fix.
	/*PTGN_ASSERT(
		!game.event.window.IsSubscribed(camera),
		"Cannot create camera from entity which is already subscribed to window events"
	);*/
	camera.SubscribeToWindowEvents();
	return camera;
}

void CameraResizeScript::OnWindowResized() {
	Camera::OnWindowResize(entity, game.window.GetSize());
}

} // namespace impl

Camera CreateCamera(Scene& scene) {
	return impl::CreateCamera(scene.CreateEntity());
}

void Camera::SubscribeToWindowEvents() {
	TryAddScript<impl::CameraResizeScript>(*this);
	OnWindowResize(*this, game.window.GetSize());
}

void Camera::UnsubscribeFromWindowEvents() {
	RemoveScripts<impl::CameraResizeScript>(*this);
}

void Camera::OnWindowResize(Camera camera, V2_float size) {
	auto& info{ camera.Get<impl::CameraInfo>() };
	size *= camera.GetZoom();
	// TODO: Potentially allow this to be modified in the future.
	bool resize{ info.GetResizeToWindow() };
	bool center{ info.GetCenterOnWindow() };
	if (resize) {
		info.SetViewportSize(size);
	}
	if (center) {
		auto pos{ size * 0.5f };
		ptgn::SetPosition(camera, pos);
	}
	if (resize || center) {
		camera.RefreshBounds();
	}
}

void Camera::RefreshBounds() {
	const auto& info{ Get<impl::CameraInfo>() };
	auto clamped{ impl::CameraInfo::ClampToBounds(
		ptgn::GetPosition(*this), info.GetBoundingBoxPosition(), info.GetBoundingBoxSize(),
		info.GetViewportSize(), GetZoom()
	) };
	ptgn::SetPosition(*this, clamped);
}

Camera::Camera(const Entity& entity) : Entity{ entity } {}

void Camera::SetPixelRounding(bool enabled) {
	Get<impl::CameraInfo>().SetPixelRounding(enabled);
}

bool Camera::IsPixelRoundingEnabled() const {
	return Get<impl::CameraInfo>().GetPixelRounding();
}

V2_float Camera::GetViewportPosition() const {
	return Get<impl::CameraInfo>().GetViewportPosition();
}

V2_float Camera::GetViewportSize() const {
	return Get<impl::CameraInfo>().GetViewportSize();
}

Camera::operator Matrix4() const {
	return GetViewProjection();
}

V2_float Camera::GetBoundsPosition() const {
	return Get<impl::CameraInfo>().GetBoundingBoxPosition();
}

V2_float Camera::GetBoundsSize() const {
	return Get<impl::CameraInfo>().GetBoundingBoxSize();
}

void Camera::SetToWindow(bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
	if (continuously) {
		UnsubscribeFromWindowEvents();
	}
	info = {};
	CenterOnWindow(continuously);
	SetViewportToWindow(continuously);
}

void Camera::CenterOnWindow(bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
	if (continuously) {
		info.SetCenterOnWindow(true);
		SubscribeToWindowEvents();
	} else {
		auto center{ game.window.GetCenter() };
		ptgn::SetPosition(*this, center);
	}
}

std::array<V2_float, 4> Camera::GetWorldVertices() const {
	const auto& info{ Get<impl::CameraInfo>() };
	Rect rect{ info.GetViewportSize() };
	Transform transform{ GetTransform(*this) };
	PTGN_ASSERT(
		transform.GetScale().BothAboveZero(),
		"Cannot get world vertices for camera with negative or zero zoom"
	);
	auto world_vertices{ rect.GetWorldVertices(transform) };
	return world_vertices;
}

V2_float Camera::GetZoom() const {
	return Abs(ptgn::GetScale(*this));
}

V3_float Camera::GetOrientation() const {
	const auto& info{ Get<impl::CameraInfo>() };
	return { ptgn::GetRotation(*this), info.GetRotationY(), info.GetRotationZ() };
}

Quaternion Camera::GetQuaternion() const {
	return Quaternion::FromEuler(GetOrientation());
}

Flip Camera::GetFlip() const {
	return Get<impl::CameraInfo>().GetFlip();
}

void Camera::SetFlip(Flip new_flip) {
	Get<impl::CameraInfo>().SetFlip(new_flip);
}

void Camera::SetBounds(const V2_float& position, const V2_float& size) {
	auto& info{ Get<impl::CameraInfo>() };
	info.SetBoundingBox(position, size);
	// Reset position to ensure it is within the new bounds.
	RefreshBounds();
}

void Camera::SetViewportPosition(const V2_float& new_position) {
	auto& info{ Get<impl::CameraInfo>() };
	info.SetResizeToWindow(false);
	info.SetViewportPosition(new_position);
}

void Camera::SetViewportSize(const V2_float& new_size) {
	auto& info{ Get<impl::CameraInfo>() };
	info.SetResizeToWindow(false);
	info.SetViewportSize(new_size);
	RefreshBounds();
}

void Camera::SetZoom(V2_float zoom) {
	PTGN_ASSERT(zoom.BothAboveZero(), "New zoom cannot be negative or zero");
	zoom  = Clamp(zoom, V2_float{ epsilon<float> }, V2_float{ std::numeric_limits<float>::max() });
	zoom *= V2_float{ Sign(zoom.x), Sign(zoom.y) };
	ptgn::SetScale(*this, zoom);
}

void Camera::SetZoom(float new_zoom) {
	SetZoom(V2_float{ new_zoom });
}

void Camera::Translate(const V2_float& position_change) {
	ptgn::GetTransform(*this).Translate(position_change);
}

void Camera::Zoom(const V2_float& zoom_change) {
	auto zoom{ GetZoom() + zoom_change };
	SetZoom(zoom);
}

void Camera::Zoom(float zoom_change) {
	Zoom(V2_float{ zoom_change });
}

void Camera::Rotate(float angle_change_radians) {
	auto rotation{ GetRotation(*this) + angle_change_radians };
	ptgn::SetRotation(*this, rotation);
}

/*
void Camera::SetYaw(float angle_radians) {
	Get<impl::CameraInfo>().SetRotation(angle_radians);
}

void Camera::SetPitch(float angle_radians) {
	auto& info{ Get<impl::CameraInfo>() };
	info.orientation.y = angle_radians;
}

void Camera::SetRoll(float angle_radians) {
	auto& info{ Get<impl::CameraInfo>() };
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
*/

void Camera::SetViewportToWindow(bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
	SetZoom(1.0f);
	if (continuously) {
		info.SetResizeToWindow(true);
		SubscribeToWindowEvents();
	} else {
		SetViewportSize(game.window.GetSize());
	}
}

void Camera::Reset() {
	ptgn::SetTransform(*this, Transform{});
	auto& info{ Get<impl::CameraInfo>() };
	info = {};
	SubscribeToWindowEvents();
}

Camera& Camera::StartFollow(Entity target, const FollowConfig& config, bool force) {
	ptgn::StartFollow(*this, target, config, force);
	return *this;
}

Camera& Camera::StopFollow(bool force) {
	ptgn::StopFollow(*this, force);
	return *this;
}

V2_float Camera::GetTopLeft() const {
	const auto& info{ Get<impl::CameraInfo>() };
	auto viewport_size{ info.GetViewportSize() };
	auto half_viewport_size{ viewport_size * 0.5f };
	return ToWorldPoint(
		V2_float{ -half_viewport_size.x, -half_viewport_size.y }, GetTransform(*this)
	);
}

Camera& Camera::TranslateTo(
	const V2_float& target_position, milliseconds duration, const Ease& ease, bool force
) {
	ptgn::TranslateTo(*this, target_position, duration, ease, force);
	return *this;
}

Camera& Camera::RotateTo(float target_angle, milliseconds duration, const Ease& ease, bool force) {
	ptgn::RotateTo(*this, target_angle, duration, ease, force);
	return *this;
}

Camera& Camera::ZoomTo(
	const V2_float& target_zoom, milliseconds duration, const Ease& ease, bool force
) {
	ptgn::ScaleTo(*this, target_zoom, duration, ease, force);
	return *this;
}

Camera& Camera::Shake(
	float intensity, milliseconds duration, const ShakeConfig& config, const Ease& ease, bool force
) {
	ptgn::Shake(*this, intensity, duration, config, ease, force);
	return *this;
}

Camera& Camera::Shake(float intensity, const ShakeConfig& config, bool force) {
	ptgn::Shake(*this, intensity, config, force);
	return *this;
}

Camera& Camera::StopShake(bool force) {
	ptgn::StopShake(*this, force);
	return *this;
}

const Matrix4& Camera::GetViewProjection() const {
	return Get<impl::CameraInfo>().GetViewProjection(ptgn::GetTransform(*this), *this);
}

void Camera::PrintInfo() const {
	auto bounds_position{ GetBoundsPosition() };
	auto bounds_size{ GetBoundsSize() };
	auto orient{ GetOrientation() };
	Print(
		"center position: ", GetPosition(*this), ", viewport position: ", GetViewportPosition(),
		", viewport size: ", GetViewportSize(), ", zoom: ", GetZoom(),
		", orientation (yaw/pitch/roll) (deg): (", RadToDeg(orient.x), ", ", RadToDeg(orient.y),
		", ", RadToDeg(orient.z), "), Bounds: "
	);
	if (bounds_size.IsZero()) {
		PrintLine("none");
	} else {
		PrintLine(bounds_position, "->", bounds_position + bounds_size);
	}
}

void CameraManager::Init(impl::SceneKey scene_key) {
	scene_key_ = scene_key;
	auto& scene{ game.scene.Get<Scene>(scene_key_) };
	PTGN_ASSERT(!window && !primary);
	primary			 = CreateCamera(scene);
	window			 = CreateCamera(scene);
	primary_unzoomed = CreateCamera(scene);
	window_unzoomed	 = CreateCamera(scene);
}

void CameraManager::Reset() {
	primary.Reset();
	window.Reset();
	primary_unzoomed.Reset();
	window_unzoomed.Reset();
}

void to_json(json& j, const CameraManager& camera_manager) {
	j["scene_key"]		  = camera_manager.scene_key_;
	j["primary"]		  = camera_manager.primary;
	j["window"]			  = camera_manager.window;
	j["primary_unzoomed"] = camera_manager.primary_unzoomed;
	j["window_unzoomed"]  = camera_manager.window_unzoomed;
}

void from_json(const json& j, CameraManager& camera_manager) {
	j.at("scene_key").get_to(camera_manager.scene_key_);
	const auto& scene{ game.scene.Get<Scene>(camera_manager.scene_key_) };
	camera_manager.primary			= scene.GetEntityByUUID(j.at("primary").at("UUID"));
	camera_manager.window			= scene.GetEntityByUUID(j.at("window").at("UUID"));
	camera_manager.primary_unzoomed = scene.GetEntityByUUID(j.at("primary_unzoomed").at("UUID"));
	camera_manager.window_unzoomed	= scene.GetEntityByUUID(j.at("window_unzoomed").at("UUID"));
}

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

} // namespace ptgn