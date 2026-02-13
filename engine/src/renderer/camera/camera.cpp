#include "renderer/camera/camera.h"

#include <algorithm>
#include <array>
#include <limits>

#include "core/assert.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/animation/offsets.h"

namespace ptgn {

Transform Camera::GetTransform() const {
	return transform;
}

void Camera::Reset() {
	*this = {};
}

void Camera::Resize(const V2_float& new_size, bool disable_auto_center, bool disable_auto_resize) {
	if (auto_center) {
		SetViewportPosition({}, disable_auto_center);
	}
	if (auto_resize) {
		SetViewportSize(new_size, disable_auto_resize);
	}
}

void Camera::SetScroll(const V2_float& new_scroll_position) {
	if (GetScroll() == new_scroll_position) {
		return;
	}
	transform.SetPosition(new_scroll_position);
	ApplyBounds();
	view_dirty = true;
}

void Camera::SetScrollX(float new_scroll_x_position) {
	SetScroll({ new_scroll_x_position, GetScroll().y });
}

void Camera::SetScrollY(float new_scroll_y_position) {
	SetScroll({ GetScroll().x, new_scroll_y_position });
}

void Camera::Scroll(const V2_float& scroll_amount) {
	SetScroll(GetScroll() + scroll_amount);
}

void Camera::ScrollX(float scroll_x_amount) {
	SetScrollX(GetScroll().x + scroll_x_amount);
}

void Camera::ScrollY(float scroll_y_amount) {
	SetScrollY(GetScroll().y + scroll_y_amount);
}

void Camera::SetZoom(const V2_float& new_zoom) {
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

void Camera::SetZoom(float new_xy_zoom) {
	SetZoom(V2_float{ new_xy_zoom });
}

void Camera::SetZoomX(float new_x_zoom) {
	SetZoom(V2_float{ new_x_zoom, GetZoom().y });
}

void Camera::SetZoomY(float new_y_zoom) {
	SetZoom(V2_float{ GetZoom().x, new_y_zoom });
}

void Camera::Zoom(const V2_float& zoom_amount) {
	auto new_zoom{ GetZoom() + zoom_amount };
	SetZoom(new_zoom);
}

void Camera::Zoom(float zoom_xy_amount) {
	auto new_zoom{ GetZoom() + V2_float{ zoom_xy_amount } };
	SetZoom(new_zoom);
}

void Camera::ZoomX(float zoom_x_amount) {
	SetZoomX(GetZoom().x + zoom_x_amount);
}

void Camera::ZoomY(float zoom_y_amount) {
	SetZoomY(GetZoom().y + zoom_y_amount);
}

void Camera::SetRotation(float new_rotation) {
	if (NearlyEqual(GetRotation(), new_rotation)) {
		return;
	}
	transform.SetRotation(new_rotation);
	view_dirty = true;
}

void Camera::Rotate(float rotation_amount) {
	SetRotation(GetRotation() + rotation_amount);
}

V2_float Camera::GetScroll() const {
	return transform.GetPosition();
}

V2_float Camera::GetZoom() const {
	auto scale{ transform.GetScale() };
	PTGN_ASSERT(scale.BothAboveZero(), "Cannot divide by negative or zero camera scale");
	return 1.0f / scale;
}

float Camera::GetRotation() const {
	return transform.GetRotation();
}

std::array<V2_float, 4> Camera::GetWorldVertices() const {
	Rect rect{ GetViewportSize() };
	auto t{ transform };
	auto world_vertices{ rect.GetWorldVertices(t) };
	return world_vertices;
}

V2_float Camera::ApplyBounds(const V2_float& scroll) const {
	if (bounding_box_size.IsZero()) {
		return scroll;
	}

	V2_float clamped_scroll;

	V2_float display_size{ GetDisplaySize() };
	V2_float half_display{ display_size * 0.5f };

	V2_float min{ bounding_box_position };
	V2_float max{ bounding_box_position + bounding_box_size };
	PTGN_ASSERT(min.x < max.x && min.y < max.y, "Bounding box min must be below maximum");

	const auto clamp_axis = [&](std::size_t axis) {
		if (display_size[axis] >= bounding_box_size[axis]) {
			// Center.
			clamped_scroll[axis] = bounding_box_position[axis] + bounding_box_size[axis] * 0.5f;
		} else {
			clamped_scroll[axis] = std::clamp(
				scroll[axis], min[axis] + half_display[axis], max[axis] - half_display[axis]
			);
		}
	};

	clamp_axis(0);
	clamp_axis(1);

	return clamped_scroll;
}

void Camera::ApplyBounds() {
	SetScroll(ApplyBounds(GetScroll()));
}

void Camera::SetViewport(const V2_float& new_viewport_position, const V2_float& new_viewport_size) {
	SetViewportPosition(new_viewport_position);
	SetViewportSize(new_viewport_size);
}

void Camera::CenterOnViewport(const V2_float& new_viewport_size) {
	SetViewportPosition({});
	SetViewportSize(new_viewport_size);
}

bool Camera::WillAutoCenter() const {
	return auto_center;
}

bool Camera::WillAutoResize() const {
	return auto_resize;
}

void Camera::SetViewportPosition(const V2_float& new_viewport_position, bool disable_auto_center) {
	if (disable_auto_center) {
		auto_center = false;
	}
	if (viewport_position == new_viewport_position) {
		return;
	}
	viewport_position = new_viewport_position;
	projection_dirty  = true;
}

void Camera::SetViewportSize(const V2_float& new_viewport_size, bool disable_auto_resize) {
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

V2_float Camera::GetViewportSize() const {
	return viewport_size;
}

V2_float Camera::GetViewportPosition() const {
	return viewport_position;
}

V2_float Camera::GetDisplaySize() const {
	PTGN_ASSERT(GetZoom().BothAboveZero(), "Cannot get display size of camera with zero zoom");
	return viewport_size / GetZoom();
}

void Camera::SetBounds(const V2_float& new_bounding_position, const V2_float& new_bounding_size) {
	if (bounding_box_position == new_bounding_position && bounding_box_size == new_bounding_size) {
		return;
	}
	bounding_box_position = new_bounding_position;
	bounding_box_size	  = new_bounding_size;
	ApplyBounds();
	view_dirty = true;
}

V2_float Camera::GetBoundsPosition() const {
	return bounding_box_position;
}

V2_float Camera::GetBoundsSize() const {
	return bounding_box_size;
}

void Camera::SetPixelRounding(bool enabled) {
	if (pixel_rounding == enabled) {
		return;
	}
	pixel_rounding	 = enabled;
	view_dirty		 = true;
	projection_dirty = true;
}

bool Camera::GetPixelRounding() const {
	return pixel_rounding;
}

const Matrix4& Camera::GetView(const Entity& camera) const {
	auto current_offsets{ GetOffset(camera) };

	view_dirty = view_dirty || transform.IsDirty() || offsets != current_offsets;

	if (view_dirty) {
		RecalculateView(current_offsets);
	}
	return view;
}

const Matrix4& Camera::GetProjection() const {
	if (projection_dirty) {
		RecalculateProjection();
	}
	return projection;
}

const Matrix4& Camera::GetViewProjection(const Entity& camera) const {
	auto current_offsets{ GetOffset(camera) };

	view_dirty = view_dirty || transform.IsDirty() || offsets != current_offsets;

	// Must be set before calling RecalculateView, as it may reset view_dirty to false.
	bool update_vp{ view_dirty || projection_dirty };

	if (view_dirty) {
		RecalculateView(current_offsets);
	}

	if (projection_dirty) {
		RecalculateProjection();
	}

	if (update_vp) {
		RecalculateViewProjection();
	}

	return view_projection;
}

void Camera::RecalculateViewProjection() const {
	view_projection = projection * view;
}

void Camera::RecalculateView(const Transform& current_offsets) const {
	Transform t{ transform };

	t.Translate(current_offsets.GetPosition());
	t.Rotate(current_offsets.GetRotation());

	if (!current_offsets.GetPosition().IsZero()) {
		t.SetPosition(ApplyBounds(t.GetPosition()));
	}

	if (pixel_rounding) {
		t.SetPosition(Round(t.GetPosition()));
	}

	view = Matrix4::MakeInverseTransform(t);

	offsets = current_offsets;
	transform.ClearDirtyFlags();
	view_dirty = false;
}

void Camera::RecalculateProjection() const {
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

} // namespace ptgn