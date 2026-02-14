#include "renderer/camera/camera.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/camera/viewport.h"
#include "renderer/renderer.h"
#include "runtime/animation/offsets.h"
#include "runtime/ecs/components/entity_transform.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

namespace ptgn {

namespace impl {

void CameraResizeScript::OnEvent(EventDispatcher d) {
	d.Dispatch<GameResized>([this](auto& e) {
		auto& c{ entity.Get<Camera>() };
		c.viewport.position = {};
		c.viewport.size		= e.size;
		ApplyCameraBounds(entity);
	});
}

V2_float ApplyCameraBounds(Entity camera, V2_float scroll) {
	auto& c{ camera.Get<impl::Camera>() };
	if (!c.bounding_box) {
		return scroll;
	}

	auto& bounding_box{ *c.bounding_box };

	V2_float clamped_scroll;

	V2_float display_size{ GetCameraDisplaySize(camera) };
	V2_float half_display{ display_size * 0.5f };

	V2_float min{ bounding_box.position };
	V2_float max{ bounding_box.position + bounding_box.size };
	PTGN_ASSERT(min.x < max.x && min.y < max.y, "Bounding box min must be below maximum");

	const auto clamp_axis = [&](std::size_t axis) {
		if (display_size[axis] >= static_cast<float>(bounding_box.size[axis])) {
			// Center.
			clamped_scroll[axis] = static_cast<float>(bounding_box.position[axis]) +
								   static_cast<float>(bounding_box.size[axis]) * 0.5f;
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

void ApplyCameraBounds(Entity camera) {
	SetScroll(camera, ApplyCameraBounds(camera, GetScroll(camera)));
}

void RecalculateViewProjection(Entity camera) {
	auto& c{ camera.Get<impl::Camera>() };

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

	V2_float size{ c.pixel_rounding ? Round(c.viewport.size) : c.viewport.size };

	auto half_size{ flip_dir * size * 0.5f };

	c.projection =
		Matrix4::Orthographic(c.viewport.position - half_size, c.viewport.position + half_size);

	Transform t{ GetTransform(camera) };

	auto current_offsets{ GetOffset(camera) };

	t.Translate(current_offsets.GetPosition());
	t.Rotate(current_offsets.GetRotation());

	t.SetPosition(ApplyCameraBounds(camera, t.GetPosition()));

	if (c.pixel_rounding) {
		t.SetPosition(Round(t.GetPosition()));
	}

	c.view = Matrix4::MakeInverseTransform(t);

	c.view_projection = c.projection * c.view;
}

} // namespace impl

void SetScroll(Entity camera, V2_float new_scroll_position) {
	if (GetScroll(camera) == new_scroll_position) {
		return;
	}
	SetPosition(camera, new_scroll_position);
	impl::ApplyCameraBounds(camera);
}

void SetScrollX(Entity camera, float new_scroll_x_position) {
	SetScroll(camera, { new_scroll_x_position, GetScroll(camera).y });
}

void SetScrollY(Entity camera, float new_scroll_y_position) {
	SetScroll(camera, { GetScroll(camera).x, new_scroll_y_position });
}

void Scroll(Entity camera, V2_float scroll_amount) {
	SetScroll(camera, GetScroll(camera) + scroll_amount);
}

void ScrollX(Entity camera, float scroll_x_amount) {
	SetScrollX(camera, GetScroll(camera).x + scroll_x_amount);
}

void ScrollY(Entity camera, float scroll_y_amount) {
	SetScrollY(camera, GetScroll(camera).y + scroll_y_amount);
}

void SetZoom(Entity camera, V2_float new_zoom) {
	auto clamped{ Clamp(
		new_zoom, V2_float{ 1000.0f * epsilon<float> },
		V2_float{ std::numeric_limits<float>::max() }
	) };
	if (GetZoom(camera) == clamped) {
		return;
	}
	PTGN_ASSERT(clamped.BothAboveZero(), "Cannot set negative or zero zoom");
	SetScale(camera, 1.0f / clamped);
	impl::ApplyCameraBounds(camera);
}

void SetZoom(Entity camera, float new_xy_zoom) {
	SetZoom(camera, V2_float{ new_xy_zoom });
}

void SetZoomX(Entity camera, float new_x_zoom) {
	SetZoom(camera, V2_float{ new_x_zoom, GetZoom(camera).y });
}

void SetZoomY(Entity camera, float new_y_zoom) {
	SetZoom(camera, V2_float{ GetZoom(camera).x, new_y_zoom });
}

void Zoom(Entity camera, V2_float zoom_amount) {
	auto new_zoom{ GetZoom(camera) + zoom_amount };
	SetZoom(camera, new_zoom);
}

void Zoom(Entity camera, float zoom_xy_amount) {
	auto new_zoom{ GetZoom(camera) + V2_float{ zoom_xy_amount } };
	SetZoom(camera, new_zoom);
}

void ZoomX(Entity camera, float zoom_x_amount) {
	SetZoomX(camera, GetZoom(camera).x + zoom_x_amount);
}

void ZoomY(Entity camera, float zoom_y_amount) {
	SetZoomY(camera, GetZoom(camera).y + zoom_y_amount);
}

V2_float GetScroll(Entity camera) {
	return GetPosition(camera);
}

V2_float GetZoom(Entity camera) {
	auto scale{ GetScale(camera) };
	PTGN_ASSERT(scale.BothAboveZero(), "Cannot divide by negative or zero camera scale");
	return 1.0f / scale;
}

std::array<V2_float, 4> GetCameraWorldVertices(Entity camera) {
	Rect rect{ GetCameraViewport(camera).size };
	auto transform{ GetTransform(camera) };
	auto world_vertices{ rect.GetWorldVertices(transform) };
	return world_vertices;
}

void SetCameraViewport(Entity camera, Viewport viewport) {
	auto& c{ camera.Get<impl::Camera>() };
	RemoveScript<impl::CameraResizeScript>(camera);
	c.viewport.position = viewport.position;
	if (c.viewport.size == viewport.size) {
		return;
	}
	c.viewport.size = viewport.size;
	impl::ApplyCameraBounds(camera);
}

Viewport GetCameraViewport(Entity camera) {
	return camera.Get<impl::Camera>().viewport;
}

V2_float GetCameraDisplaySize(Entity camera) {
	PTGN_ASSERT(
		GetZoom(camera).BothAboveZero(), "Cannot get display size of camera with zero zoom"
	);
	return camera.Get<impl::Camera>().viewport.size / GetZoom(camera);
}

void SetCameraBounds(Entity camera, std::optional<Viewport> bounds) {
	auto& c{ camera.Get<impl::Camera>() };
	c.bounding_box = bounds;
	impl::ApplyCameraBounds(camera);
}

std::optional<Viewport> GetCameraBounds(Entity camera) {
	return camera.Get<impl::Camera>().bounding_box;
}

void SetPixelRounding(Entity camera, bool enabled) {
	auto& c{ camera.Get<impl::Camera>() };
	c.pixel_rounding = enabled;
}

bool GetPixelRounding(Entity camera) {
	return camera.Get<impl::Camera>().pixel_rounding;
}

const Matrix4& GetView(Entity camera) {
	return camera.Get<impl::Camera>().view;
}

const Matrix4& GetProjection(Entity camera) {
	return camera.Get<impl::Camera>().projection;
}

const Matrix4& GetViewProjection(Entity camera) {
	return camera.Get<impl::Camera>().view_projection;
}

void ResetCamera(Entity camera) {
	PTGN_ASSERT(camera.Has<impl::Camera>());
	camera.Add<Transform>();
	camera.Add<impl::Camera>();
	AddScript<impl::CameraResizeScript>(camera);
}

namespace impl {

Entity CreateCamera(Entity camera, const Renderer& renderer) {
	camera.Add<Transform>();
	camera.Add<impl::Camera>();
	AddScript<impl::CameraResizeScript>(camera);
	SetCameraViewport(camera, { {}, renderer.GetGameSize() });
	return camera;
}

} // namespace impl

Entity CreateCamera(Scene& scene, const Renderer& renderer) {
	return impl::CreateCamera(scene.CreateEntity(), renderer);
}

Entity CreateCamera(Scene& scene, const Renderer& renderer, V2_float viewport_size) {
	auto camera{ CreateCamera(scene, renderer) };
	SetCameraViewport(camera, { {}, viewport_size });
	return camera;
}

} // namespace ptgn