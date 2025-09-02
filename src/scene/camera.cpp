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

Transform CameraInstance::GetTransform() const {
	return transform;
}

Transform& CameraInstance::GetTransform() {
	return transform;
}

void CameraInstance::Reset() {
	*this = {};
}

void CameraInstance::Resize(const V2_float& new_size) {
	if (auto_center) {
		SetViewportPosition({}, false);
	}
	if (auto_resize) {
		SetViewportSize(new_size, false);
	}
}

void CameraInstance::SetScroll(const V2_float& new_scroll_position) {
	if (GetScroll() == new_scroll_position) {
		return;
	}
	transform.SetPosition(new_scroll_position);
	ApplyBounds();
	view_dirty = true;
}

void CameraInstance::SetScroll(std::size_t index, float new_scroll_position) {
	PTGN_ASSERT(index == 0 || index == 1, "Axis index out of range");
	if (index == 0) {
		SetScrollX(new_scroll_position);
		return;
	}
	SetScrollY(new_scroll_position);
}

void CameraInstance::SetScrollX(float new_scroll_x_position) {
	SetScroll({ new_scroll_x_position, GetScroll().y });
}

void CameraInstance::SetScrollY(float new_scroll_y_position) {
	SetScroll({ GetScroll().x, new_scroll_y_position });
}

void CameraInstance::Scroll(const V2_float& scroll_amount) {
	SetScroll(GetScroll() + scroll_amount);
}

void CameraInstance::ScrollX(float scroll_x_amount) {
	SetScrollX(GetScroll().x + scroll_x_amount);
}

void CameraInstance::ScrollY(float scroll_y_amount) {
	SetScrollY(GetScroll().y + scroll_y_amount);
}

void CameraInstance::SetZoom(const V2_float& new_zoom) {
	auto clamped{ Clamp(
		new_zoom, V2_float{ 1000.0f * epsilon<float> },
		V2_float{ std::numeric_limits<float>::max() }
	) };
	if (GetZoom() == clamped) {
		return;
	}
	PTGN_ASSERT(clamped.BothAboveZero(), "Cannot set negative or zero zoom");
	transform.SetScale(1.0f / clamped);
	ApplyBounds();
	view_dirty = true;
}

void CameraInstance::SetZoom(float new_xy_zoom) {
	SetZoom(V2_float{ new_xy_zoom });
}

void CameraInstance::SetZoomX(float new_x_zoom) {
	SetZoom(V2_float{ new_x_zoom, GetZoom().y });
}

void CameraInstance::SetZoomY(float new_y_zoom) {
	SetZoom(V2_float{ GetZoom().x, new_y_zoom });
}

void CameraInstance::Zoom(const V2_float& zoom_amount) {
	SetZoom(GetZoom() + zoom_amount);
}

void CameraInstance::Zoom(float zoom_xy_amount) {
	SetZoom(GetZoom() + V2_float{ zoom_xy_amount });
}

void CameraInstance::ZoomX(float zoom_x_amount) {
	SetZoomX(GetZoom().x + zoom_x_amount);
}

void CameraInstance::ZoomY(float zoom_y_amount) {
	SetZoomY(GetZoom().y + zoom_y_amount);
}

void CameraInstance::SetRotation(float new_rotation) {
	if (NearlyEqual(GetRotation(), new_rotation)) {
		return;
	}
	transform.SetRotation(new_rotation);
	view_dirty = true;
}

void CameraInstance::Rotate(float rotation_amount) {
	SetRotation(GetRotation() + rotation_amount);
}

V2_float CameraInstance::GetScroll() const {
	return transform.GetPosition();
}

V2_float CameraInstance::GetZoom() const {
	auto scale{ transform.GetScale() };
	PTGN_ASSERT(scale.BothAboveZero(), "Cannot divide by negative or zero camera scale");
	return 1.0f / scale;
}

float CameraInstance::GetRotation() const {
	return transform.GetRotation();
}

std::array<V2_float, 4> CameraInstance::GetWorldVertices() const {
	Rect rect{ GetViewportSize() };
	auto t{ transform };
	auto world_vertices{ rect.GetWorldVertices(t) };
	return world_vertices;
}

void CameraInstance::ApplyBounds() {
	if (bounding_box_size.IsZero()) {
		return;
	}

	V2_float display_size{ GetDisplaySize() };
	V2_float half_display{ display_size * 0.5f };

	V2_float min{ bounding_box_position };
	V2_float max{ bounding_box_position + bounding_box_size };
	PTGN_ASSERT(min.x < max.x && min.y < max.y, "Bounding box min must be below maximum");

	const auto clamp_axis = [&](std::size_t axis) {
		if (display_size[axis] >= bounding_box_size[axis]) {
			// Center.
			SetScroll(axis, bounding_box_position[axis] + bounding_box_size[axis] * 0.5f);
		} else {
			SetScroll(
				axis, std::clamp(
						  GetScroll()[axis], min[axis] + half_display[axis],
						  max[axis] - half_display[axis]
					  )
			);
		}
	};

	clamp_axis(0);
	clamp_axis(1);
}

void CameraInstance::SetViewport(
	const V2_float& new_viewport_position, const V2_float& new_viewport_size
) {
	SetViewportPosition(new_viewport_position);
	SetViewportSize(new_viewport_size);
}

void CameraInstance::CenterOnViewport(const V2_float& new_viewport_size) {
	SetViewportPosition({});
	SetViewportSize(new_viewport_size);
}

void CameraInstance::SetViewportPosition(
	const V2_float& new_viewport_position, bool disable_auto_center
) {
	if (disable_auto_center) {
		auto_center = false;
	}
	if (viewport_position == new_viewport_position) {
		return;
	}
	viewport_position = new_viewport_position;
	projection_dirty  = true;
}

void CameraInstance::SetViewportSize(const V2_float& new_viewport_size, bool disable_auto_resize) {
	if (disable_auto_resize) {
		auto_resize = false;
	}
	if (viewport_size == new_viewport_size) {
		return;
	}
	viewport_size = new_viewport_size;
	ApplyBounds();
	projection_dirty = true;
}

V2_float CameraInstance::GetViewportSize() const {
	return viewport_size;
}

V2_float CameraInstance::GetViewportPosition() const {
	return viewport_position;
}

V2_float CameraInstance::GetDisplaySize() const {
	PTGN_ASSERT(GetZoom().BothAboveZero(), "Cannot get display size of camera with zero zoom");
	return viewport_size / GetZoom();
}

void CameraInstance::SetBounds(
	const V2_float& new_bounding_position, const V2_float& new_bounding_size
) {
	if (bounding_box_position == new_bounding_position && bounding_box_size == new_bounding_size) {
		return;
	}
	bounding_box_position = new_bounding_position;
	bounding_box_size	  = new_bounding_size;
	ApplyBounds();
	view_dirty = true;
}

V2_float CameraInstance::GetBoundsPosition() const {
	return bounding_box_position;
}

V2_float CameraInstance::GetBoundsSize() const {
	return bounding_box_size;
}

void CameraInstance::SetPixelRounding(bool enabled) {
	if (pixel_rounding == enabled) {
		return;
	}
	pixel_rounding	 = enabled;
	view_dirty		 = true;
	projection_dirty = true;
}

bool CameraInstance::GetPixelRounding() const {
	return pixel_rounding;
}

const Matrix4& CameraInstance::GetView() const {
	if (view_dirty) {
		RecalculateView();
	}
	return view;
}

const Matrix4& CameraInstance::GetProjection() const {
	if (projection_dirty) {
		RecalculateProjection();
	}
	return projection;
}

const Matrix4& CameraInstance::GetViewProjection() const {
	view_dirty = view_dirty || transform.IsDirty();

	bool update_vp{ view_dirty || projection_dirty };

	if (view_dirty) {
		RecalculateView();
	}

	if (projection_dirty) {
		RecalculateProjection();
	}

	if (update_vp) {
		RecalculateViewProjection();
	}

	return view_projection;
}

void CameraInstance::RecalculateViewProjection() const {
	view_projection = projection * view;
}

void CameraInstance::RecalculateView() const {
	Transform t{ transform };
	// TODO: Add shake and other offsets to position and rotation.
	// TODO: Apply clamp to bounds to offset position.

	if (pixel_rounding) {
		t.SetPosition(Round(t.GetPosition()));
	}

	view = Matrix4::MakeInverseTransform(t);

	transform.ClearDirtyFlags();
	view_dirty = false;
}

void CameraInstance::RecalculateProjection() const {
	V2_float flip_dir{ 1.0f, 1.0f };

	/*
	// TODO: Consider adding flip in the future.
	Flip flip{ Flip::None };

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
	*/

	V2_float size{ pixel_rounding ? Round(viewport_size) : viewport_size };

	auto half_size{ flip_dir * size * 0.5f };

	projection =
		Matrix4::Orthographic(viewport_position - half_size, viewport_position + half_size);

	projection_dirty = false;
}

void CameraGameSizeResizeScript::OnGameSizeChanged() {
	auto game_size{ game.renderer.GetGameSize() };
	Camera::Resize(entity, game_size);
}

} // namespace impl

void Camera::Subscribe() {
	TryAddScript<impl::CameraGameSizeResizeScript>(*this);
	V2_int game_size{ game.renderer.GetGameSize() };
	Resize(*this, game_size);
}

void Camera::Unsubscribe() {
	RemoveScripts<impl::CameraGameSizeResizeScript>(*this);
}

void Camera::Resize(Camera camera, const V2_float& new_size) {
	camera.Get<impl::CameraInstance>().Resize(new_size);
}

Camera::Camera(const Entity& entity) : Entity{ entity } {}

void Camera::SetPixelRounding(bool enabled) {
	Get<impl::CameraInstance>().SetPixelRounding(enabled);
}

bool Camera::IsPixelRoundingEnabled() const {
	return Get<impl::CameraInstance>().GetPixelRounding();
}

void Camera::SetViewport(const V2_float& new_viewport_position, const V2_float& new_viewport_size) {
	Get<impl::CameraInstance>().SetViewport(new_viewport_position, new_viewport_size);
}

void Camera::SetViewportPosition(const V2_float& new_viewport_position) {
	Get<impl::CameraInstance>().SetViewportPosition(new_viewport_position);
}

void Camera::SetViewportSize(const V2_float& new_viewport_size) {
	Get<impl::CameraInstance>().SetViewportSize(new_viewport_size);
}

void Camera::CenterOnViewport(const V2_float& new_viewport_size) {
	Get<impl::CameraInstance>().CenterOnViewport(new_viewport_size);
}

V2_float Camera::GetViewportPosition() const {
	return Get<impl::CameraInstance>().GetViewportPosition();
}

V2_float Camera::GetViewportSize() const {
	return Get<impl::CameraInstance>().GetViewportSize();
}

V2_float Camera::GetDisplaySize() const {
	return Get<impl::CameraInstance>().GetDisplaySize();
}

Camera::operator Matrix4() const {
	return GetViewProjection();
}

V2_float Camera::GetBoundsPosition() const {
	return Get<impl::CameraInstance>().GetBoundsPosition();
}

V2_float Camera::GetBoundsSize() const {
	return Get<impl::CameraInstance>().GetBoundsSize();
}

std::array<V2_float, 4> Camera::GetWorldVertices() const {
	return Get<impl::CameraInstance>().GetWorldVertices();
}

V2_float Camera::GetZoom() const {
	return Get<impl::CameraInstance>().GetZoom();
}

void Camera::SetBounds(const V2_float& position, const V2_float& size) {
	Get<impl::CameraInstance>().SetBounds(position, size);
}

void Camera::SetScroll(const V2_float& new_scroll) {
	Get<impl::CameraInstance>().SetScroll(new_scroll);
}

void Camera::Scroll(const V2_float& scroll_amount) {
	Get<impl::CameraInstance>().Scroll(scroll_amount);
}

void Camera::ScrollX(float scroll_x_amount) {
	Get<impl::CameraInstance>().ScrollX(scroll_x_amount);
}

void Camera::ScrollY(float scroll_y_amount) {
	Get<impl::CameraInstance>().ScrollY(scroll_y_amount);
}

void Camera::SetZoom(const V2_float& new_zoom) {
	Get<impl::CameraInstance>().SetZoom(new_zoom);
}

void Camera::SetZoom(float new_zoom) {
	Get<impl::CameraInstance>().SetZoom(new_zoom);
}

void Camera::Zoom(const V2_float& zoom_amount) {
	Get<impl::CameraInstance>().Zoom(zoom_amount);
}

void Camera::Zoom(float zoom_xy_amount) {
	Get<impl::CameraInstance>().Zoom(zoom_xy_amount);
}

void Camera::ZoomX(float zoom_x_amount) {
	Get<impl::CameraInstance>().ZoomX(zoom_x_amount);
}

void Camera::ZoomY(float zoom_y_amount) {
	Get<impl::CameraInstance>().ZoomY(zoom_y_amount);
}

void Camera::SetRotation(float new_rotation) {
	Get<impl::CameraInstance>().SetRotation(new_rotation);
}

void Camera::Rotate(float rotation_amount) {
	Get<impl::CameraInstance>().Rotate(rotation_amount);
}

void Camera::Reset() {
	Get<impl::CameraInstance>().Reset();
	Subscribe();
}

Transform Camera::GetTransform() const {
	return Get<impl::CameraInstance>().GetTransform();
}

Transform& Camera::GetTransform() {
	return Get<impl::CameraInstance>().GetTransform();
}

const Matrix4& Camera::GetViewProjection() const {
	return Get<impl::CameraInstance>().GetViewProjection();
}

Camera CreateCamera(Manager& manager) {
	Camera camera{ manager.CreateEntity() };
	camera.Add<impl::CameraInstance>();
	camera.Reset();
	return camera;
}

} // namespace ptgn