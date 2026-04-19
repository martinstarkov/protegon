
#include "runtime/scene/scene_camera.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/tolerance.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/pipeline/viewport_event.h"
#include "runtime/animation/offsets.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/render_target.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/script.h"

namespace ptgn {

namespace impl {

RenderCamera::RenderCamera(const Camera& world_camera) : camera{ world_camera } {}

RenderCamera::RenderCamera(SceneCamera scene_camera) :
	uuid{ scene_camera.GetUUID() },
	depth{ GetDepth(scene_camera) },
	camera{ scene_camera.operator ptgn::Camera() },
	clear_color{ scene_camera.GetClearColor() } {
	if (auto parent_rt{ scene_camera.template TryGet<impl::ParentRenderTarget>() }) {
		render_target = parent_rt->render_target;
	}
}

void CameraResizeScript::OnEvent(Event event) {
	event.Dispatch<ptgn::event::GameResized>([this](auto& resized) {
		auto& camera{ entity.Get<CameraData>() };
		camera.viewport = { {}, resized.size };
		// PTGN_LOG("SceneCamera ", entity, " received game resize: ", resized.size);
		ApplyCameraBounds(SceneCamera{ entity });
	});
}

V2_float ApplyCameraBounds(SceneCamera camera, V2_float scroll) {
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

void ApplyCameraBounds(SceneCamera camera) {
	camera.SetScroll(ApplyCameraBounds(camera, camera.GetScroll()));
}

void RecalculateCameraViewProjection(SceneCamera camera) {
	auto& c{ camera.Get<impl::CameraData>() };

	auto viewport_size{ c.viewport.size };

	// TODO: Consider adding flip in the future.
	// V2_float flip_dir{ 1.0f, 1.0f };
	// Flip flip{ Flip::None };
	// switch (flip) {
	//	case Flip::None:	   break;
	//	case Flip::Vertical:   flip_dir.y = -1.0f; break;
	//	case Flip::Horizontal: flip_dir.x = -1.0f; break;
	//	case Flip::Both:
	//		flip_dir.x = -1.0f;
	//		flip_dir.y = -1.0f;
	//		break;
	//	default: PTGN_ERROR("Unrecognized flip state");
	//}
	// viewport_size *= flip_dir;

	Transform camera_transform{ GetTransform(camera) };

	auto current_offsets{ GetOffset(camera) };

	camera_transform.Translate(current_offsets.GetPosition());
	camera_transform.Rotate(current_offsets.GetRotation());

	camera_transform.SetPosition(ApplyCameraBounds(camera, camera_transform.GetPosition()));

	c.view_projection_data =
		GetOrthographicViewProjection(camera_transform, viewport_size, c.pixel_rounding);
}

} // namespace impl

SceneCamera::SceneCamera(Entity entity) : Entity{ entity } {}

SceneCamera::operator Camera() const {
	return Camera{ .transform{ GetTransform(*this) },
				   .viewport{ GetViewport() },
				   .view_projection{ GetViewProjection() } };
}

SceneCamera& SceneCamera::SetScroll(V2_float new_scroll_position) {
	if (GetScroll() == new_scroll_position) {
		return *this;
	}
	SetPosition(*this, new_scroll_position);
	impl::ApplyCameraBounds(*this);
	return *this;
}

SceneCamera& SceneCamera::SetScrollX(float new_scroll_x_position) {
	return SetScroll({ new_scroll_x_position, GetScroll().y });
}

SceneCamera& SceneCamera::SetScrollY(float new_scroll_y_position) {
	return SetScroll({ GetScroll().x, new_scroll_y_position });
}

SceneCamera& SceneCamera::Scroll(V2_float scroll_amount) {
	return SetScroll(GetScroll() + scroll_amount);
}

SceneCamera& SceneCamera::ScrollX(float scroll_x_amount) {
	return SetScrollX(GetScroll().x + scroll_x_amount);
}

SceneCamera& SceneCamera::ScrollY(float scroll_y_amount) {
	return SetScrollY(GetScroll().y + scroll_y_amount);
}

SceneCamera& SceneCamera::SetZoom(V2_float new_zoom) {
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

SceneCamera& SceneCamera::SetZoom(float new_xy_zoom) {
	return SetZoom(V2_float{ new_xy_zoom });
}

SceneCamera& SceneCamera::SetZoomX(float new_x_zoom) {
	return SetZoom(V2_float{ new_x_zoom, GetZoom().y });
}

SceneCamera& SceneCamera::SetZoomY(float new_y_zoom) {
	return SetZoom(V2_float{ GetZoom().x, new_y_zoom });
}

SceneCamera& SceneCamera::Zoom(V2_float zoom_amount) {
	auto new_zoom{ GetZoom() + zoom_amount };
	return SetZoom(new_zoom);
}

SceneCamera& SceneCamera::Zoom(float zoom_xy_amount) {
	auto new_zoom{ GetZoom() + V2_float{ zoom_xy_amount } };
	return SetZoom(new_zoom);
}

SceneCamera& SceneCamera::ZoomX(float zoom_x_amount) {
	return SetZoomX(GetZoom().x + zoom_x_amount);
}

SceneCamera& SceneCamera::ZoomY(float zoom_y_amount) {
	return SetZoomY(GetZoom().y + zoom_y_amount);
}

V2_float SceneCamera::GetScroll() const {
	return GetPosition(*this);
}

V2_float SceneCamera::GetZoom() const {
	auto scale{ GetScale(*this) };
	PTGN_ASSERT(scale.BothAboveZero(), "Cannot divide by negative or zero camera scale");
	return 1.0f / scale;
}

std::array<V2_float, 4> SceneCamera::GetWorldVertices() const {
	Rect rect{ GetViewport().size };
	auto transform{ GetTransform(*this) };
	auto world_vertices{ rect.GetWorldVertices(transform) };
	return world_vertices;
}

SceneCamera& SceneCamera::SetViewport(Viewport viewport) {
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

Viewport SceneCamera::GetViewport() const {
	return Get<impl::CameraData>().viewport;
}

V2_float SceneCamera::GetDisplaySize() const {
	PTGN_ASSERT(GetZoom().BothAboveZero(), "Cannot get display size of camera with zero zoom");
	return Get<impl::CameraData>().viewport.size / GetZoom();
}

SceneCamera& SceneCamera::SetBounds(std::optional<Viewport> bounds) {
	auto& c{ Get<impl::CameraData>() };
	c.bounding_box = bounds;
	impl::ApplyCameraBounds(*this);
	return *this;
}

std::optional<Viewport> SceneCamera::GetBounds() const {
	return Get<impl::CameraData>().bounding_box;
}

SceneCamera& SceneCamera::SetPixelRounding(bool enabled) {
	auto& c{ Get<impl::CameraData>() };
	c.pixel_rounding = enabled;
	return *this;
}

bool SceneCamera::GetPixelRounding() const {
	return Get<impl::CameraData>().pixel_rounding;
}

const Matrix4& SceneCamera::GetView() const {
	return Get<impl::CameraData>().view_projection_data.view;
}

const Matrix4& SceneCamera::GetProjection() const {
	return Get<impl::CameraData>().view_projection_data.projection;
}

const Matrix4& SceneCamera::GetViewProjection() const {
	return Get<impl::CameraData>().view_projection_data.view_projection;
}

SceneCamera& SceneCamera::Reset() {
	PTGN_ASSERT(Has<impl::CameraData>());
	Add<Transform>();
	Add<impl::CameraData>();
	AddScript<impl::CameraResizeScript>(*this);
	return *this;
}

LayerMask SceneCamera::GetIncludeMask() const {
	return GetOrDefault<impl::CameraMask>().include;
}

LayerMask SceneCamera::GetExcludeMask() const {
	return GetOrDefault<impl::CameraMask>().exclude;
}

SceneCamera& SceneCamera::SetMasks(LayerMask include, LayerMask exclude) {
	auto& c	  = TryAdd<impl::CameraMask>();
	c.include = include;
	c.exclude = exclude;
	return *this;
}

SceneCamera& SceneCamera::SetIncludeMask(LayerMask include) {
	TryAdd<impl::CameraMask>().include = include;
	return *this;
}

SceneCamera& SceneCamera::SetExcludeMask(LayerMask exclude) {
	TryAdd<impl::CameraMask>().exclude = exclude;
	return *this;
}

SceneCamera& SceneCamera::AddIncludeMasks(LayerMask layers_to_add) {
	TryAdd<impl::CameraMask>().include |= layers_to_add;
	return *this;
}

SceneCamera& SceneCamera::RemoveIncludeMasks(LayerMask layers_to_remove) {
	TryAdd<impl::CameraMask>().include &= ~layers_to_remove;
	return *this;
}

SceneCamera& SceneCamera::AddExcludeMasks(LayerMask layers_to_add) {
	TryAdd<impl::CameraMask>().exclude |= layers_to_add;
	return *this;
}

SceneCamera& SceneCamera::RemoveExcludeMasks(LayerMask layers_to_remove) {
	TryAdd<impl::CameraMask>().exclude &= ~layers_to_remove;
	return *this;
}

SceneCamera& SceneCamera::ClearMasks() {
	Remove<impl::CameraMask>();
	return *this;
}

bool SceneCamera::IsVisible(Entity entity) const {
	auto entity_mask = GetMask(entity);
	auto include	 = GetIncludeMask();
	auto exclude	 = GetExcludeMask();

	bool in_include = (entity_mask & include) != 0;
	bool in_exclude = (entity_mask & exclude) != 0;

	return in_include && !in_exclude || IsUI(*this) && IsUI(entity);
}

SceneCamera& SceneCamera::SetParentRenderTarget(const RenderTarget& render_target) {
	PTGN_ASSERT(
		render_target, "Cannot set camera parent render target to an invalid render target"
	);
	Add<impl::ParentRenderTarget>(render_target);
	return *this;
}

SceneCamera& SceneCamera::SetParentRenderTarget() {
	Add<impl::ParentRenderTarget>(GetScene().GetRenderTarget());
	return *this;
}

void SceneCamera::SetClearColor(std::optional<Color> clear_color) {
	if (clear_color.has_value()) {
		Add<impl::ClearColor>(*clear_color);
	} else {
		Remove<impl::ClearColor>();
	}
}

std::optional<Color> SceneCamera::GetClearColor() const {
	if (auto color{ TryGet<impl::ClearColor>() }) {
		return *color;
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

V2_float GetCameraParentRenderTargetScale(
	const Scene& scene, const std::optional<SceneCamera>& camera
) {
	SceneCamera cam{ camera.value_or(scene.ctx().camera) };
	RenderTarget render_target;
	if (auto parent_rt = cam.TryGet<impl::ParentRenderTarget>()) {
		render_target = parent_rt->render_target;
	} else {
		render_target = scene.GetRenderTarget();
	}
	PTGN_ASSERT(render_target, "Failed to find a valid render target when calculating scale");
	auto zoom{ cam.GetZoom() };
	PTGN_ASSERT(zoom.BothAboveZero(), "SceneCamera zoom cannot be negative or zero");
	return render_target.GetScale() * zoom;
}

void AddCameraComponents(SceneCamera camera, const RenderContext& renderer) {
	camera.Add<Transform>();
	camera.Add<impl::CameraData>();
	camera.SetViewport({ {}, renderer.GetGameSize() });
	AddScript<impl::CameraResizeScript>(camera);
}

} // namespace impl

SceneCamera CreateCamera(Scene& scene) {
	SceneCamera camera{ scene.CreateEntity() };
	impl::AddCameraComponents(camera, scene.ctx().renderer);
	return camera;
}

SceneCamera CreateCamera(Scene& scene, V2_float viewport_size) {
	auto camera{ CreateCamera(scene) };
	camera.SetViewport({ {}, viewport_size });
	return camera;
}

} // namespace ptgn