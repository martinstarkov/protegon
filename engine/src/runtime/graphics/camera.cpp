
#include "runtime/graphics/camera.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/pipeline/viewport_event.h"
#include "runtime/animation/offsets.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/render_target.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"

namespace ptgn {

namespace impl {

void CameraResizeScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::GameResized>([this](auto& resized) {
		auto& camera{ entity.Get<CameraData>() };
		camera.viewport = { {}, resized.size };
		// PTGN_LOG("Camera ", entity, " received game resize: ", resized.size);
		ApplyCameraBounds(Camera{ entity });
	});
}

V2_float ApplyCameraBounds(Camera camera, V2_float scroll) {
	auto& c{ camera.Get<impl::CameraData>() };
	if (!c.bounding_box) {
		return scroll;
	}

	auto& bounding_box{ *c.bounding_box };

	V2_float clamped_scroll;

	V2_float display_size{ camera.GetDisplaySize() };
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

void ApplyCameraBounds(Camera camera) {
	camera.SetScroll(ApplyCameraBounds(camera, camera.GetScroll()));
}

void RecalculateCameraViewProjection(Camera camera) {
	auto& c{ camera.Get<impl::CameraData>() };

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

	V2_float size{ c.pixel_rounding ? FastRound(c.viewport.size) : c.viewport.size };

	auto half_size{ flip_dir * size * 0.5f };

	V2_float min{ -half_size };
	V2_float max{ half_size };

	c.projection = Matrix4::Orthographic(min, max);

	Transform t{ GetTransform(camera) };

	auto current_offsets{ GetOffset(camera) };

	t.Translate(current_offsets.GetPosition());
	t.Rotate(current_offsets.GetRotation());

	t.SetPosition(ApplyCameraBounds(camera, t.GetPosition()));

	if (c.pixel_rounding) {
		t.SetPosition(FastRound(t.GetPosition()));
	}

	c.view = Matrix4::MakeInverseTransform(t);

	c.view_projection = c.projection * c.view;
}

} // namespace impl

Camera::Camera(Entity entity) : Entity{ entity } {}

Camera& Camera::SetScroll(V2_float new_scroll_position) {
	if (GetScroll() == new_scroll_position) {
		return *this;
	}
	SetPosition(*this, new_scroll_position);
	impl::ApplyCameraBounds(*this);
	return *this;
}

Camera& Camera::SetScrollX(float new_scroll_x_position) {
	return SetScroll({ new_scroll_x_position, GetScroll().y });
}

Camera& Camera::SetScrollY(float new_scroll_y_position) {
	return SetScroll({ GetScroll().x, new_scroll_y_position });
}

Camera& Camera::Scroll(V2_float scroll_amount) {
	return SetScroll(GetScroll() + scroll_amount);
}

Camera& Camera::ScrollX(float scroll_x_amount) {
	return SetScrollX(GetScroll().x + scroll_x_amount);
}

Camera& Camera::ScrollY(float scroll_y_amount) {
	return SetScrollY(GetScroll().y + scroll_y_amount);
}

Camera& Camera::SetZoom(V2_float new_zoom) {
	auto clamped{ Clamp(
		new_zoom, V2_float{ 1000.0f * kEpsilon<float> },
		V2_float{ std::numeric_limits<float>::max() }
	) };
	if (GetZoom() == clamped) {
		return *this;
	}
	PTGN_ASSERT(clamped.BothAboveZero(), "Cannot set negative or zero zoom");
	SetScale(*this, 1.0f / clamped);
	impl::ApplyCameraBounds(*this);
	return *this;
}

Camera& Camera::SetZoom(float new_xy_zoom) {
	return SetZoom(V2_float{ new_xy_zoom });
}

Camera& Camera::SetZoomX(float new_x_zoom) {
	return SetZoom(V2_float{ new_x_zoom, GetZoom().y });
}

Camera& Camera::SetZoomY(float new_y_zoom) {
	return SetZoom(V2_float{ GetZoom().x, new_y_zoom });
}

Camera& Camera::Zoom(V2_float zoom_amount) {
	auto new_zoom{ GetZoom() + zoom_amount };
	return SetZoom(new_zoom);
}

Camera& Camera::Zoom(float zoom_xy_amount) {
	auto new_zoom{ GetZoom() + V2_float{ zoom_xy_amount } };
	return SetZoom(new_zoom);
}

Camera& Camera::ZoomX(float zoom_x_amount) {
	return SetZoomX(GetZoom().x + zoom_x_amount);
}

Camera& Camera::ZoomY(float zoom_y_amount) {
	return SetZoomY(GetZoom().y + zoom_y_amount);
}

V2_float Camera::GetScroll() const {
	return GetPosition(*this);
}

V2_float Camera::GetZoom() const {
	auto scale{ GetScale(*this) };
	PTGN_ASSERT(scale.BothAboveZero(), "Cannot divide by negative or zero camera scale");
	return 1.0f / scale;
}

std::array<V2_float, 4> Camera::GetWorldVertices() const {
	Rect rect{ GetViewport().size };
	auto transform{ GetTransform(*this) };
	auto world_vertices{ rect.GetWorldVertices(transform) };
	return world_vertices;
}

Camera& Camera::SetViewport(Viewport viewport) {
	auto& c{ Get<impl::CameraData>() };
	RemoveScript<impl::CameraResizeScript>(*this);
	c.viewport.position = viewport.position;
	if (c.viewport.size == viewport.size) {
		return *this;
	}
	c.viewport.size = viewport.size;
	impl::ApplyCameraBounds(*this);
	return *this;
}

Viewport Camera::GetViewport() const {
	return Get<impl::CameraData>().viewport;
}

V2_float Camera::GetDisplaySize() const {
	PTGN_ASSERT(GetZoom().BothAboveZero(), "Cannot get display size of camera with zero zoom");
	return Get<impl::CameraData>().viewport.size / GetZoom();
}

Camera& Camera::SetBounds(std::optional<Viewport> bounds) {
	auto& c{ Get<impl::CameraData>() };
	c.bounding_box = bounds;
	impl::ApplyCameraBounds(*this);
	return *this;
}

std::optional<Viewport> Camera::GetBounds() const {
	return Get<impl::CameraData>().bounding_box;
}

Camera& Camera::SetPixelRounding(bool enabled) {
	auto& c{ Get<impl::CameraData>() };
	c.pixel_rounding = enabled;
	return *this;
}

bool Camera::GetPixelRounding() const {
	return Get<impl::CameraData>().pixel_rounding;
}

const Matrix4& Camera::GetView() const {
	return Get<impl::CameraData>().view;
}

const Matrix4& Camera::GetProjection() const {
	return Get<impl::CameraData>().projection;
}

const Matrix4& Camera::GetViewProjection() const {
	return Get<impl::CameraData>().view_projection;
}

Camera& Camera::Reset() {
	PTGN_ASSERT(Has<impl::CameraData>());
	Add<Transform>();
	Add<impl::CameraData>();
	AddScript<impl::CameraResizeScript>(*this);
	return *this;
}

LayerMask Camera::GetIncludeMask() const {
	return GetOrDefault<impl::CameraMask>().include;
}

LayerMask Camera::GetExcludeMask() const {
	return GetOrDefault<impl::CameraMask>().exclude;
}

Camera& Camera::SetMasks(LayerMask include, LayerMask exclude) {
	auto& c	  = TryAdd<impl::CameraMask>();
	c.include = include;
	c.exclude = exclude;
	return *this;
}

Camera& Camera::SetIncludeMask(LayerMask include) {
	TryAdd<impl::CameraMask>().include = include;
	return *this;
}

Camera& Camera::SetExcludeMask(LayerMask exclude) {
	TryAdd<impl::CameraMask>().exclude = exclude;
	return *this;
}

Camera& Camera::AddIncludeMasks(LayerMask layers_to_add) {
	TryAdd<impl::CameraMask>().include |= layers_to_add;
	return *this;
}

Camera& Camera::RemoveIncludeMasks(LayerMask layers_to_remove) {
	TryAdd<impl::CameraMask>().include &= ~layers_to_remove;
	return *this;
}

Camera& Camera::AddExcludeMasks(LayerMask layers_to_add) {
	TryAdd<impl::CameraMask>().exclude |= layers_to_add;
	return *this;
}

Camera& Camera::RemoveExcludeMasks(LayerMask layers_to_remove) {
	TryAdd<impl::CameraMask>().exclude &= ~layers_to_remove;
	return *this;
}

Camera& Camera::ClearMasks() {
	Remove<impl::CameraMask>();
	return *this;
}

bool Camera::IsVisible(Entity entity) const {
	auto entity_mask = GetMask(entity);
	auto include	 = GetIncludeMask();
	auto exclude	 = GetExcludeMask();

	bool in_include = (entity_mask & include) != 0;
	bool in_exclude = (entity_mask & exclude) != 0;

	return in_include && !in_exclude || IsUI(*this) && IsUI(entity);
}

Camera& Camera::SetParentRenderTarget(const RenderTarget& render_target) {
	PTGN_ASSERT(
		render_target, "Cannot set camera parent render target to an invalid render target"
	);
	Add<impl::ParentRenderTarget>(render_target);
	return *this;
}

Camera& Camera::SetParentRenderTarget() {
	Add<impl::ParentRenderTarget>(GetScene().GetRenderTarget());
	return *this;
}

void Camera::SetClearColor(std::optional<Color> clear_color) {
	if (clear_color.has_value()) {
		Add<ClearColor>(*clear_color);
	} else {
		Remove<ClearColor>();
	}
}

std::optional<Color> Camera::GetClearColor() const {
	if (auto color{ TryGet<ClearColor>() }) {
		return color->value;
	} else {
		return {};
	}
}

void SetUI(Entity entity, bool ui_layer) {
	if (ui_layer) {
		entity.Add<impl::UILayer>();
		SetMask(entity, kLayersNone);
	} else {
		entity.Remove<impl::UILayer>();
		SetMask(entity, kLayersAll);
	}
}

bool IsUI(Entity entity) {
	return entity.Has<impl::UILayer>();
}

LayerMask GetMask(Entity entity) {
	return entity.GetOrDefault<impl::RenderMask>().layers;
}

void SetMask(Entity entity, LayerMask mask) {
	entity.TryAdd<impl::RenderMask>().layers = mask;
}

void AddMasks(Entity entity, LayerMask layers_to_add) {
	entity.TryAdd<impl::RenderMask>().layers |= layers_to_add;
}

void RemoveMasks(Entity entity, LayerMask layers_to_remove) {
	entity.TryAdd<impl::RenderMask>().layers &= ~layers_to_remove;
}

void ClearMasks(Entity entity) {
	entity.Remove<impl::RenderMask>();
}

bool HasAnyMask(Entity entity, LayerMask test) {
	return (GetMask(entity) & test) != 0;
}

bool HasAllMasks(Entity entity, LayerMask test) {
	auto m{ GetMask(entity) };
	return (m & test) == test;
}

namespace impl {

V2_float GetCameraParentRenderTargetScale(const Scene& scene, const std::optional<Camera>& camera) {
	Camera cam{ camera.value_or(scene.ctx().camera) };
	RenderTarget render_target;
	if (auto parent_rt = cam.TryGet<impl::ParentRenderTarget>()) {
		render_target = parent_rt->render_target;
	} else {
		render_target = scene.GetRenderTarget();
	}
	PTGN_ASSERT(render_target, "Failed to find a valid render target when calculating scale");
	auto zoom{ cam.GetZoom() };
	PTGN_ASSERT(zoom.BothAboveZero(), "Camera zoom cannot be negative or zero");
	return render_target.GetScale() * zoom;
}

void AddCameraComponents(Camera camera, const RenderContext& renderer) {
	camera.Add<Transform>();
	camera.Add<impl::CameraData>();
	camera.SetViewport({ {}, renderer.GetGameSize() });
	AddScript<impl::CameraResizeScript>(camera);
}

} // namespace impl

Camera CreateCamera(Scene& scene) {
	Camera camera{ scene.CreateEntity() };
	impl::AddCameraComponents(camera, scene.ctx().renderer);
	return camera;
}

Camera CreateCamera(Scene& scene, V2_float viewport_size) {
	auto camera{ CreateCamera(scene) };
	camera.SetViewport({ {}, viewport_size });
	return camera;
}

} // namespace ptgn