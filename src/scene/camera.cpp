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
#include "core/script_interfaces.h"
#include "debug/log.h"
#include "math/geometry.h"
#include "math/geometry/rect.h"
#include "math/math.h"
#include "math/matrix4.h"
#include "math/quaternion.h"
#include "math/vector2.h"
#include "math/vector3.h"
#include "renderer/api/flip.h"
#include "renderer/renderer.h"

namespace ptgn {

namespace impl {

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

void CameraInfo::SetResizeMode(CameraResizeMode resize) {
	if (resize_mode == resize) {
		return;
	}
	resize_mode		 = resize;
	projection_dirty = true;
}

void CameraInfo::SetCenterMode(CameraCenterMode center) const {
	if (center_mode == center) {
		return;
	}
	center_mode = center;
	view_dirty	= true;
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

V2_float CameraInfo::GetViewportSize() const {
	return viewport_size;
}

V2_float CameraInfo::GetBoundingBoxPosition() const {
	return bounding_box_position;
}

V2_float CameraInfo::GetBoundingBoxSize() const {
	return bounding_box_size;
}

CameraResizeMode CameraInfo::GetResizeMode() const {
	return resize_mode;
}

CameraCenterMode CameraInfo::GetCenterMode() const {
	return center_mode;
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

	bool position_changed{ current.GetPosition() != previous.GetPosition() };

	if (position_changed) {
		SetCenterMode(CameraCenterMode::Custom);
	}

	// Either view is dirty, the camera has been offset (due to shake or other effects), or the
	// current position of the camera differs from its previous position (for instance, as a result
	// of a system changing the position of the camera entity externally).
	bool update_view{ view_dirty || position_changed || offset_transform != Transform{} ||
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

	PTGN_ASSERT(zoom.BothAboveZero());

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

void CameraLogicalResolutionResizeScript::OnLogicalResolutionChanged() {
	auto logical_resolution{ game.renderer.GetLogicalResolution() };
	Camera::OnResolutionChanged(
		entity, logical_resolution, CameraResizeMode::LogicalResolution,
		CameraCenterMode::LogicalResolution
	);
}

void CameraPhysicalResolutionResizeScript::OnPhysicalResolutionChanged() {
	auto physical_resolution{ game.renderer.GetPhysicalResolution() };
	Camera::OnResolutionChanged(
		entity, physical_resolution, CameraResizeMode::PhysicalResolution,
		CameraCenterMode::PhysicalResolution
	);
}

} // namespace impl

void Camera::SubscribeToResolutionEvents(
	CameraResizeMode resize_mode, CameraCenterMode center_mode
) {
	if (resize_mode == CameraResizeMode::Custom) {
		return;
	}
	TryAddScript<impl::CameraLogicalResolutionResizeScript>(*this);
	TryAddScript<impl::CameraPhysicalResolutionResizeScript>(*this);
	V2_int resolution;
	if (resize_mode == CameraResizeMode::LogicalResolution) {
		resolution = game.renderer.GetLogicalResolution();
	} else if (resize_mode == CameraResizeMode::PhysicalResolution) {
		resolution = game.renderer.GetPhysicalResolution();
	}
	OnResolutionChanged(*this, resolution, resize_mode, center_mode);
}

void Camera::UnsubscribeFromResolutionEvents() {
	RemoveScripts<impl::CameraLogicalResolutionResizeScript>(*this);
	RemoveScripts<impl::CameraPhysicalResolutionResizeScript>(*this);
}

void Camera::OnResolutionChanged(
	Camera camera, V2_float size, CameraResizeMode resize_mode, CameraCenterMode center_mode
) {
	auto& info{ camera.Get<impl::CameraInfo>() };
	size *= camera.GetZoom();

	bool position_changed{ GetPosition(camera) != info.previous.GetPosition() };

	if (position_changed) {
		info.SetCenterMode(CameraCenterMode::Custom);
	}

	bool resize{ info.GetResizeMode() == resize_mode };
	bool center{ info.GetCenterMode() == center_mode };
	if (resize) {
		info.SetViewportSize(size);
	}
	if (center) {
		auto pos{ size * 0.5f };
		ptgn::SetPosition(camera, pos);
		info.view_dirty = true;
		// This prevents the mode from center_mode being switched to CameraCenterMode::Custom when
		// re-calculating the view projection matrix.
		info.previous.SetPosition(pos);
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

void Camera::SetToLogicalResolution(bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
	if (continuously) {
		UnsubscribeFromResolutionEvents();
	}
	info = {};
	SetCenterMode(CameraCenterMode::LogicalResolution, continuously);
	SetResizeMode(CameraResizeMode::LogicalResolution, continuously);
}

void Camera::SetToPhysicalResolution(bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
	if (continuously) {
		UnsubscribeFromResolutionEvents();
	}
	info = {};
	SetCenterMode(CameraCenterMode::PhysicalResolution, continuously);
	SetResizeMode(CameraResizeMode::PhysicalResolution, continuously);
}

void Camera::SetCenterMode(CameraCenterMode center_mode, bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
	if (continuously) {
		info.SetCenterMode(center_mode);
		if (center_mode != CameraCenterMode::Custom) {
			V2_float resolution;
			if (center_mode == CameraCenterMode::LogicalResolution) {
				resolution = game.renderer.GetLogicalResolution();
			} else if (center_mode == CameraCenterMode::PhysicalResolution) {
				resolution = game.renderer.GetPhysicalResolution();
			}

			auto pos{ GetZoom() * resolution * 0.5f };
			ptgn::SetPosition(*this, pos);
			// This prevents the mode from center_mode being switched to CameraCenterMode::Custom
			// when re-calculating the view projection matrix.
			info.previous.SetPosition(pos);

			SubscribeToResolutionEvents(info.GetResizeMode(), center_mode);
		}
	} else if (center_mode != CameraCenterMode::Custom) {
		V2_float resolution;
		if (center_mode == CameraCenterMode::LogicalResolution) {
			resolution = game.renderer.GetLogicalResolution();
		} else if (center_mode == CameraCenterMode::PhysicalResolution) {
			resolution = game.renderer.GetPhysicalResolution();
		}
		auto center{ resolution / 2.0f };
		ptgn::SetPosition(*this, center);
	}
}

void Camera::SetResizeMode(CameraResizeMode resize_mode, bool continuously) {
	auto& info{ Get<impl::CameraInfo>() };
	SetZoom(1.0f);
	if (continuously) {
		info.SetResizeMode(resize_mode);
		if (resize_mode != CameraResizeMode::Custom) {
			SubscribeToResolutionEvents(resize_mode, info.GetCenterMode());
		}
	} else if (resize_mode != CameraResizeMode::Custom) {
		V2_float resolution;
		if (resize_mode == CameraResizeMode::LogicalResolution) {
			resolution = game.renderer.GetLogicalResolution();
		} else if (resize_mode == CameraResizeMode::PhysicalResolution) {
			resolution = game.renderer.GetPhysicalResolution();
		}
		PTGN_ASSERT(
			!resolution.IsZero(), "Failed to find a valid resolution for the resizing camera"
		);
		SetViewportSize(resolution);
	}
}

std::array<V2_float, 4> Camera::GetWorldVertices() const {
	const auto& info{ Get<impl::CameraInfo>() };
	Transform transform{ GetTransform(*this) };
	auto zoom{ transform.GetScale() };
	transform.SetScale(V2_float{ 1.0f, 1.0f });
	Rect rect{ info.GetViewportSize() / zoom };
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

void Camera::SetViewportSize(const V2_float& new_size) {
	auto& info{ Get<impl::CameraInfo>() };
	info.SetResizeMode(CameraResizeMode::Custom);
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

void Camera::Zoom(const V2_float& zoom_change) {
	auto zoom{ GetZoom() + zoom_change };
	SetZoom(zoom);
}

void Camera::Zoom(float zoom_change) {
	Zoom(V2_float{ zoom_change });
}

void Camera::Reset() {
	ptgn::SetTransform(*this, Transform{});
	auto& info{ Get<impl::CameraInfo>() };
	info = {};
	SubscribeToResolutionEvents(
		CameraResizeMode::LogicalResolution, CameraCenterMode::LogicalResolution
	);
}

V2_float Camera::GetTopLeft() const {
	const auto& info{ Get<impl::CameraInfo>() };
	auto viewport_size{ info.GetViewportSize() };
	auto half_viewport_size{ viewport_size * 0.5f };
	return ToWorldPoint(
		V2_float{ -half_viewport_size.x, -half_viewport_size.y }, GetTransform(*this)
	);
}

const Matrix4& Camera::GetViewProjection() const {
	return Get<impl::CameraInfo>().GetViewProjection(ptgn::GetTransform(*this), *this);
}

void Camera::PrintInfo() const {
	auto bounds_position{ GetBoundsPosition() };
	auto bounds_size{ GetBoundsSize() };
	auto orient{ GetOrientation() };
	Print(
		"center position: ", GetPosition(*this), ", viewport size: ", GetViewportSize(),
		", zoom: ", GetZoom(), ", orientation (yaw/pitch/roll) (deg): (", RadToDeg(orient.x), ", ",
		RadToDeg(orient.y), ", ", RadToDeg(orient.z), "), Bounds: "
	);
	if (bounds_size.IsZero()) {
		PrintLine("none");
	} else {
		PrintLine(bounds_position, "->", bounds_position + bounds_size);
	}
}

V2_float ToWorldPoint(const V2_float& screen_point, const Camera& camera) {
	Transform camera_transform{ GetTransform(camera) };
	auto zoom{ camera_transform.GetScale() };
	PTGN_ASSERT(zoom.BothAboveZero());
	V2_float unzoomed		= (screen_point - camera.GetViewportSize() / 2.0f) / zoom;
	V2_float rotated		= unzoomed.Rotated(camera_transform.GetRotation());
	V2_float world_position = rotated + camera_transform.GetPosition();
	return world_position;
}

Camera CreateCamera(Manager& manager) {
	Camera camera{ manager.CreateEntity() };
	camera.Add<impl::CameraInfo>();
	camera.Reset();
	return camera;
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

/*
// To move camera according to mouse drag (in 3D):
void CameraController::OnMouseMoveEvent([[maybe_unused]] const MouseMoveEvent& e) {

	static bool first_mouse = true;

	if (game.input.MousePressed(Mouse::Left)) {
		const MouseMoveEvent& mouse = static_cast<const MouseMoveEvent&>(e);
		if (!first_mouse) {
			V2_float offset = mouse.GetDifference();

			info.size = logical_resolution;

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