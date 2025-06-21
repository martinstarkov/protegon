#include "scene/camera.h"

#include <algorithm>
#include <array>
#include <functional>
#include <limits>

#include "common/assert.h"
#include "components/offsets.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/time.h"
#include "core/window.h"
#include "debug/log.h"
#include "events/event_handler.h"
#include "events/events.h"
#include "math/easing.h"
#include "math/geometry.h"
#include "math/math.h"
#include "math/matrix4.h"
#include "math/quaternion.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "rendering/api/flip.h"
#include "rendering/api/origin.h"
#include "rendering/batching/render_data.h"
#include "rendering/renderer.h"
#include "scene/scene.h"
#include "scene/scene_key.h"
#include "scene/scene_manager.h"
#include "tweening/follow_config.h"
#include "tweening/shake_config.h"
#include "tweening/tween_effects.h"

namespace ptgn {

namespace impl {

void CameraInfo::SetViewport(
	const V2_float& new_viewport_position, const V2_float& new_viewport_size
) {
	viewport_position = new_viewport_position;
	viewport_size	  = new_viewport_size;
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

void CameraInfo::SetSize(const V2_float& new_size) {
	if (size == new_size) {
		return;
	}
	size			 = new_size;
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

V2_float CameraInfo::GetSize() const {
	return size;
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

void CameraInfo::UpdatePosition(const V2_float& position) {
	if (previous.position == position) {
		return;
	}
	previous.position = position;
	view_dirty		  = true;
}

void CameraInfo::UpdateRotation(float rotation) {
	if (previous.rotation == rotation) {
		return;
	}
	previous.rotation = rotation;
	view_dirty		  = true;
}

void CameraInfo::UpdateScale(const V2_float& scale) {
	if (previous.scale == scale) {
		return;
	}
	previous.scale	 = scale;
	projection_dirty = true;
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
	bool update_view{ view_dirty || current.position != previous.position ||
					  offset_transform != Transform{} ||
					  !NearlyEqual(current.rotation, previous.rotation) };

	bool update_projection{ projection_dirty || current.scale != previous.scale };

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
	V3_float position{ current.position.x, current.position.y, position_z };
	V3_float orientation{ current.rotation, orientation_y, orientation_z };

	position.x	  += offset_transform.position.x;
	position.y	  += offset_transform.position.y;
	orientation.x += offset_transform.rotation;

	if (!offset_transform.position.IsZero()) {
		auto zoom{ Abs(current.scale) };
		// Reclamp offset position to ensure camera shake does not move the camera out of
		// bounds.
		auto clamped{ ClampToBounds(
			{ position.x, position.y }, bounding_box_position, bounding_box_size, size, zoom
		) };

		position.x = clamped.x;
		position.y = clamped.y;
	}

	if (pixel_rounding) {
		position = Round(position);
	}

	V3_float mirror_position{ -position.x, -position.y, position.z };

	Quaternion quat_orientation{ Quaternion::FromEuler(orientation) };
	view	   = Matrix4::Translate(quat_orientation.ToMatrix4(), mirror_position);
	view_dirty = false;
}

void CameraInfo::RecalculateProjection(const Transform& current) const {
	auto zoom{ Abs(current.scale) };
	PTGN_ASSERT(zoom.x > 0.0f && zoom.y > 0.0f);
	V2_float extents{ (size * 0.5f) / zoom };
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
	const V2_float& camera_size, const V2_float& camera_zoom
) {
	if (bounding_box_size.IsZero()) {
		return position;
	}

	V2_float min{ bounding_box_position };
	V2_float max{ bounding_box_position + bounding_box_size };
	PTGN_ASSERT(min.x < max.x && min.y < max.y, "Bounding box min must be below maximum");

	V2_float center{ Midpoint(min, max) };

	// TODO: Incoporate yaw, i.e. data.orientation.x into the bounds using sin and cos.
	V2_float real_size{ camera_size / camera_zoom };
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

Camera CreateCamera(const Entity& entity) {
	Camera camera{ entity };
	camera.Add<Transform>();
	camera.Add<impl::CameraInfo>();
	PTGN_ASSERT(
		!game.event.window.IsSubscribed(camera),
		"Cannot create camera from entity which is already subscribed to window events"
	);
	camera.SubscribeToWindowEvents();
	return camera;
}

} // namespace impl

Camera CreateCamera(Scene& scene) {
	return impl::CreateCamera(scene.CreateEntity());
}

V2_float Camera::ZoomIfNeeded(const V2_float& zoomed_coordinate) const {
	const auto& camera_manager{ game.scene.GetCurrent().camera };

	V2_float center;

	if (!*this) {
		auto camera{ game.renderer.GetRenderData().render_state.camera };
		if (!camera) {
			return zoomed_coordinate;
		}
		center = camera.GetPosition();
	} else {
		center = GetPosition();
	}

	V2_float zoom{ 1.0f, 1.0f };

	if (*this == camera_manager.window_unzoomed) {
		zoom = camera_manager.window.GetZoom();
	} else if (*this == camera_manager.primary_unzoomed) {
		zoom = camera_manager.primary.GetZoom();
	} else {
		return zoomed_coordinate;
	}

	PTGN_ASSERT(zoom.x != 0.0f && zoom.y != 0.0f);

	return (zoomed_coordinate - center) * zoom + center;
}

void Camera::SubscribeToWindowEvents() {
	if (game.event.window.IsSubscribed(*this)) {
		return;
	}
	std::function<void(const WindowResizedEvent&)> f = [*this](const WindowResizedEvent& e
													   ) mutable {
		OnWindowResize(e.size);
	};
	game.event.window.Subscribe(WindowEvent::Resized, *this, f);
	OnWindowResize(game.window.GetSize());
}

void Camera::UnsubscribeFromWindowEvents() {
	game.event.window.Unsubscribe(*this);
}

void Camera::OnWindowResize(V2_float size) {
	auto& info{ Get<impl::CameraInfo>() };
	size *= GetZoom();
	// TODO: Potentially allow this to be modified in the future.
	info.SetViewport({}, game.window.GetSize());
	bool resize{ info.GetResizeToWindow() };
	bool center{ info.GetCenterOnWindow() };
	if (resize) {
		info.SetSize(size);
	}
	if (center) {
		auto pos{ size * 0.5f };
		SetPosition(pos);
		info.UpdatePosition(pos);
	}
	if (resize || center) {
		RefreshBounds();
	}
}

void Camera::RefreshBounds() {
	auto& info{ Get<impl::CameraInfo>() };
	auto clamped{ impl::CameraInfo::ClampToBounds(
		Entity::GetPosition(), info.GetBoundingBoxPosition(), info.GetBoundingBoxSize(),
		info.GetSize(), GetZoom()
	) };
	SetPosition(clamped);
	info.UpdatePosition(clamped);
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

V2_float Camera::GetPosition(Origin origin) const {
	PTGN_ASSERT(Has<impl::CameraInfo>());
	const auto& info{ Get<impl::CameraInfo>() };
	auto position{ Entity::GetPosition() };
	auto zoom{ GetZoom() };
	PTGN_ASSERT(zoom.x != 0.0f && zoom.y != 0.0f);
	auto size{ info.GetSize() };
	auto offset{ GetOriginOffset(origin, size / zoom) };
	return position + offset;
}

void Camera::SetToWindow(bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
	if (continuously) {
		UnsubscribeFromWindowEvents();
	}
	info = {};
	CenterOnWindow(continuously);
	SetSizeToWindow(continuously);
}

void Camera::CenterOnArea(const V2_float& new_size) {
	SetSize(new_size);
	auto pos{ new_size / 2.0f };
	auto& info{ Get<impl::CameraInfo>() };
	SetPosition(pos);
	info.UpdatePosition(pos);
}

V2_float Camera::ScaleToCamera(const V2_float& screen_relative_size) const {
	const auto& info{ Get<impl::CameraInfo>() };

	auto camera_zoom{ GetZoom() };
	PTGN_ASSERT(camera_zoom.x != 0.0f && camera_zoom.y != 0.0f);

	auto viewport_size{ info.GetViewportSize() };
	PTGN_ASSERT(viewport_size.x != 0.0f && viewport_size.y != 0.0f);

	auto camera_size{ info.GetSize() };

	// Ratio of camera size (in world units) to viewport size (in pixels).
	auto pixels_to_world{ (camera_size / viewport_size) / camera_zoom };

	// Convert screen size in pixels to world units.
	auto world_size{ screen_relative_size * pixels_to_world };

	return world_size;
}

float Camera::ScaleToCamera(float screen_relative_size) const {
	return ScaleToCamera(V2_float{ screen_relative_size }).x;
}

V2_float Camera::ScaleToScreen(const V2_float& camera_relative_size) const {
	const auto& info{ Get<impl::CameraInfo>() };

	auto camera_size{ info.GetSize() };
	PTGN_ASSERT(camera_size.x != 0.0f && camera_size.y != 0.0f);

	auto camera_zoom{ GetZoom() };
	auto viewport_size{ info.GetViewportSize() };

	// Scale camera size by zoom.
	auto zoomed_size{ camera_relative_size * camera_zoom };

	// Convert to screen pixels.
	auto pixels_per_world_unit{ viewport_size / camera_size };
	auto screen_size{ zoomed_size * pixels_per_world_unit };

	return screen_size;
}

float Camera::ScaleToScreen(float camera_relative_size) const {
	return ScaleToScreen(V2_float{ camera_relative_size }).x;
}

V2_float Camera::TransformToCamera(const V2_float& screen_relative_coordinate) const {
	// TODO: Take into account camera rotation.
	const auto& info{ Get<impl::CameraInfo>() };

	auto camera_zoom{ GetZoom() };
	PTGN_ASSERT(camera_zoom.x != 0.0f && camera_zoom.y != 0.0f);

	auto viewport_pos{ info.GetViewportPosition() };
	auto viewport_size{ info.GetViewportSize() };
	PTGN_ASSERT(viewport_size.x != 0.0f && viewport_size.y != 0.0f);

	// Normalize screen coordinates to [0, 1] range.
	auto normalized_coordinate{ (screen_relative_coordinate - viewport_pos) / viewport_size };

	auto camera_size{ info.GetSize() };

	// Scale normalized coordinates to camera size and apply zoom.
	auto world_coordinate{ (normalized_coordinate * camera_size) / camera_zoom };

	// Translate to camera position (using bottom right as origin because viewport is relative to
	// top left).
	world_coordinate += GetPosition(Origin::BottomRight);

	return world_coordinate;
}

V2_float Camera::TransformToScreen(const V2_float& camera_relative_coordinate) const {
	// TODO: Take into account camera rotation.
	const auto& info{ Get<impl::CameraInfo>() };

	auto camera_size{ info.GetSize() };
	PTGN_ASSERT(camera_size.x != 0.0f && camera_size.y != 0.0f);

	auto camera_zoom{ GetZoom() };

	auto viewport_pos{ info.GetViewportPosition() };
	auto viewport_size{ info.GetViewportSize() };

	V2_float relative_coordinate{ (camera_relative_coordinate - GetPosition(Origin::BottomRight)) *
								  camera_zoom };

	V2_float normalized_coordinate{ relative_coordinate / camera_size };

	V2_float screen_coordinate{ viewport_pos + normalized_coordinate * viewport_size };

	return screen_coordinate;
}

void Camera::CenterOnWindow(bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
	if (continuously) {
		info.SetCenterOnWindow(true);
		SubscribeToWindowEvents();
	} else {
		auto center{ game.window.GetCenter() };
		SetPosition(center);
		info.UpdatePosition(center);
	}
}

std::array<V2_float, 4> Camera::GetVertices() const {
	auto zoom{ GetZoom() };
	PTGN_ASSERT(zoom.x != 0.0f && zoom.y != 0.0f);
	return impl::GetVertices(
		{ GetPosition(Origin::Center), GetRotation() }, GetSize() / zoom, Origin::Center
	);
}

V2_float Camera::GetSize() const {
	return Get<impl::CameraInfo>().GetSize();
}

V2_float Camera::GetZoom() const {
	return Abs(GetScale());
}

V3_float Camera::GetOrientation() const {
	const auto& info{ Get<impl::CameraInfo>() };
	return { Entity::GetRotation(), info.GetRotationY(), info.GetRotationZ() };
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

void Camera::SetSize(const V2_float& new_size) {
	auto& info{ Get<impl::CameraInfo>() };
	info.SetResizeToWindow(false);
	info.SetSize(new_size);
	RefreshBounds();
}

void Camera::SetZoom(V2_float new_zoom) {
	PTGN_ASSERT(new_zoom.x > 0.0f && new_zoom.y > 0.0f, "New zoom cannot be negative or zero");
	new_zoom.x = std::clamp(new_zoom.x, epsilon<float>, std::numeric_limits<float>::max());
	new_zoom.y = std::clamp(new_zoom.y, epsilon<float>, std::numeric_limits<float>::max());
	auto zoom{ Entity::GetScale() };
	new_zoom *= V2_float{ Sign(zoom.x), Sign(zoom.y) };
	Entity::SetScale(new_zoom);
	auto& info{ Get<impl::CameraInfo>() };
	info.UpdateScale(new_zoom);
}

void Camera::SetZoom(float new_zoom) {
	SetZoom(V2_float{ new_zoom });
}

void Camera::Translate(const V2_float& position_change) {
	// TODO: This might boil down to not needing quaternions (since z position change is zero) but I
	// am not sure due to the sine and cosines.
	auto change{ V3_float{ position_change.x, position_change.y, 0.0f } * GetQuaternion() };
	auto old_pos{ Entity::GetPosition() };
	auto& info{ Get<impl::CameraInfo>() };
	auto new_pos{ old_pos + V2_float{ change.x, change.y } };
	SetPosition(new_pos);
	info.UpdatePosition(new_pos);
	info.SetPositionZ(info.GetPositionZ() + change.z);
}

void Camera::Zoom(const V2_float& zoom_change) {
	auto new_zoom{ GetZoom() + zoom_change };
	PTGN_ASSERT(
		new_zoom.x > 0.0f && new_zoom.y > 0.0f, "Resulting zoom cannot be negative or zero"
	);
	SetZoom(new_zoom);
}

void Camera::Zoom(float zoom_change) {
	Zoom(V2_float{ zoom_change });
}

void Camera::Rotate(float angle_change_radians) {
	auto rotation{ GetRotation() + angle_change_radians };
	SetRotation(rotation);
	auto& info{ Get<impl::CameraInfo>() };
	info.UpdateRotation(rotation);
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

void Camera::SetSizeToWindow(bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
	SetZoom(1.0f);
	if (continuously) {
		info.SetResizeToWindow(true);
		SubscribeToWindowEvents();
	} else {
		SetSize(game.window.GetSize());
	}
}

void Camera::Reset() {
	Entity::SetTransform(Transform{});
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

void Camera::PrintInfo() const {
	auto bounds_position{ GetBoundsPosition() };
	auto bounds_size{ GetBoundsSize() };
	auto orient{ GetOrientation() };
	Print(
		"center position: ", GetPosition(Origin::Center), ", size: ", GetSize(),
		", zoom: ", GetZoom(), ", orientation (yaw/pitch/roll) (deg): (", RadToDeg(orient.x), ", ",
		RadToDeg(orient.y), ", ", RadToDeg(orient.z), "), Bounds: "
	);
	if (bounds_size.IsZero()) {
		PrintLine("none");
	} else {
		PrintLine(bounds_position, "->", bounds_position + bounds_size);
	}
}

const Matrix4& Camera::GetViewProjection() const {
	return Get<impl::CameraInfo>().GetViewProjection(Entity::GetTransform(), *this);
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
	auto& scene{ game.scene.Get<Scene>(camera_manager.scene_key_) };
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