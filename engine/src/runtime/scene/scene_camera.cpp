
#include "runtime/scene/scene_camera.h"

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
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "runtime/animation/offsets.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_target.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

namespace {

V2_float ApplyCameraBounds(SceneCamera camera, V2_float position) {
	auto& c{ camera.Get<impl::CameraData>() };

	if (!c.bounding_box) {
		return position;
	}

	auto& bounding_box{ *c.bounding_box };

	V2_float clamped_scroll;

	V2_float size{ camera.GetSize() };
	V2_float half_size{ size * 0.5f };

	Rect bounds{ bounding_box.position, bounding_box.rect.GetSize(), bounding_box.origin };

	auto bounds_size{ bounds.GetSize() };

	auto clamp_axis = [&](std::size_t axis) {
		if (half_size[axis] >= bounds_size[axis]) {
			// Center.
			clamped_scroll[axis] = bounding_box.position[axis] + bounds_size[axis] * 0.5f;
		} else {
			clamped_scroll[axis] = std::clamp(
				position[axis], bounds.min[axis] + half_size[axis],
				bounds.max[axis] - half_size[axis]
			);
		}
	};

	clamp_axis(0);
	clamp_axis(1);

	return clamped_scroll;
}

} // namespace

namespace impl {

void ApplyCameraBounds(SceneCamera camera) {
	SetPosition(camera, ptgn::ApplyCameraBounds(camera, GetPosition(camera)));
}

void RecalculateCameraViewProjection(SceneCamera camera) {
	auto& c{ camera.Get<impl::CameraData>() };

	auto view_size{ camera.GetLogicalViewport().size };

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
	// view_size *= flip_dir;

	Transform camera_transform{ GetTransform(camera) };

	auto current_offsets{ GetOffset(camera) };

	camera_transform.Translate(current_offsets.position);
	camera_transform.Rotate(current_offsets.rotation);

	camera_transform.position = ptgn::ApplyCameraBounds(camera, camera_transform.position);

	c.view_projection =
		GetOrthographicViewProjection(camera_transform, view_size, c.pixel_rounding);
}

} // namespace impl

SceneCamera::SceneCamera(Entity entity) : Entity{ entity } {}

SceneCamera::operator Camera() const {
	return Camera{ .transform{ GetTransform(*this) },
				   .raw_viewport{ GetRawViewport() },
				   .viewport_space = GetViewportSpace(),
				   .view_projection{ GetViewProjection() } };
}

SceneCamera& SceneCamera::SetZoom(V2_float new_zoom) {
	auto clamped{ Clamp(
		new_zoom, V2_float{ 1000.0f * kEpsilon<float> },
		V2_float{ std::numeric_limits<float>::max() }
	) };
	if (GetZoom() == clamped) {
		return *this;
	}
	PTGN_ASSERT(clamped.IsPositive(), "Cannot set negative or zero zoom");
	SetScale(*this, 1.0f / clamped);
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

V2_float SceneCamera::GetZoom() const {
	auto scale{ GetScale(*this) };
	PTGN_ASSERT(scale.IsPositive(), "Cannot divide by negative or zero camera scale");
	return 1.0f / scale;
}

std::array<V2_float, 4> SceneCamera::GetWorldVertices() const {
	Rect rect{ GetLogicalViewport().size };
	auto transform{ GetTransform(*this) };
	auto world_vertices{ rect.GetWorldVertices(transform) };
	return world_vertices;
}

SceneCamera& SceneCamera::SetViewport(
	std::optional<Viewport> viewport, ViewportSpace viewport_space
) {
	if (viewport_space == ViewportSpace::Normalized) {
		PTGN_ASSERT(
			viewport.has_value(), "Viewport must have a value when using normalized viewport space"
		);
		PTGN_ASSERT(
			WithinRangeInclusive(viewport.value().position, 0.0f, 1.0f),
			"Viewport position must be between 0 and 1 in normalized viewport space"
		);
	}
	auto& c{ Get<impl::CameraData>() };
	c.viewport_space = viewport_space;
	c.raw_viewport	 = viewport;
	return *this;
}

std::optional<Viewport> SceneCamera::GetRawViewport() const {
	return Get<impl::CameraData>().raw_viewport;
}

Viewport SceneCamera::GetLogicalViewport() const {
	const auto& camera{ Get<impl::CameraData>() };

	auto logical_size{ GetScene().ctx().renderer.GetLogicalSize() };

	return ptgn::GetLogicalViewport(GetRawViewport(), camera.viewport_space, logical_size);
}

Viewport SceneCamera::GetDisplayViewport() const {
	const auto& camera{ Get<impl::CameraData>() };

	auto logical_size{ GetScene().ctx().renderer.GetLogicalSize() };

	auto parent{ GetRenderTarget() };

	auto target_size{ parent.GetSize() };

	return ptgn::GetDisplayViewport(
		GetRawViewport(), camera.viewport_space, logical_size, target_size,
		parent == GetScene().GetRenderTarget()
	);
}

ViewportSpace SceneCamera::GetViewportSpace() const {
	return Get<impl::CameraData>().viewport_space;
}

V2_float SceneCamera::GetSize() const {
	PTGN_ASSERT(GetZoom().IsPositive(), "Camera zoom must be positive");
	return GetLogicalViewport().size / GetZoom();
}

SceneCamera& SceneCamera::SetBounds(const std::optional<BoundingBox>& bounds) {
	auto& c{ Get<impl::CameraData>() };
	c.bounding_box = bounds;
	return *this;
}

std::optional<BoundingBox> SceneCamera::GetBounds() const {
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

const Matrix4& SceneCamera::GetViewProjection() const {
	return Get<impl::CameraData>().view_projection;
}

SceneCamera& SceneCamera::Reset() {
	Add<Transform>();
	Add<impl::CameraData>();
	return *this;
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

LayerMask SceneCamera::GetIncludeMask() const {
	return GetOrDefault<impl::CameraMask>().include;
}

LayerMask SceneCamera::GetExcludeMask() const {
	return GetOrDefault<impl::CameraMask>().exclude;
}

bool SceneCamera::CanSee(Entity entity) const {
	auto entity_mask = GetMask(entity);
	auto include	 = GetIncludeMask();
	auto exclude	 = GetExcludeMask();

	bool in_include = (entity_mask & include) != 0;
	bool in_exclude = (entity_mask & exclude) != 0;

	return (in_include && !in_exclude) || (IsUI(*this) && IsUI(entity));
}

SceneCamera& SceneCamera::SetRenderTarget(const std::optional<RenderTarget>& parent) {
	PTGN_ASSERT(
		!parent.has_value() || parent.value(),
		"Cannot set camera parent render target to an invalid render target"
	);

	auto new_parent{ parent.value_or(GetScene().GetRenderTarget()) };

	PTGN_ASSERT(
		HasDraw<RenderTarget>(new_parent) || new_parent == GetScene().GetRenderTarget(),
		"Camera parent must be a render target"
	);

	Add<impl::ParentRenderTarget>(new_parent);

	return *this;
}

RenderTarget SceneCamera::GetRenderTarget() const {
	PTGN_ASSERT(
		Has<impl::ParentRenderTarget>(),
		"Each camera must have a parent render target assigned to it"
	);

	auto parent{ Get<impl::ParentRenderTarget>().render_target };

	PTGN_ASSERT(
		HasDraw<RenderTarget>(parent) || parent == GetScene().GetRenderTarget(),
		"Camera parent must be a render target"
	);

	return parent;
}

void SceneCamera::SetClearColor(std::optional<Color> clear_color) {
	if (clear_color.has_value()) {
		Add<impl::ClearColor>(clear_color.value());
	} else {
		Remove<impl::ClearColor>();
	}
}

std::optional<Color> SceneCamera::GetClearColor() const {
	if (auto clear{ TryGet<impl::ClearColor>() }) {
		return clear->color;
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
		SetMask(entity, impl::RenderMask{}.layers);
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

SceneCamera CreateCamera(
	Scene& scene, Transform transform, std::optional<V2_float> viewport_size,
	ViewportSpace viewport_space
) {
	SceneCamera camera{ scene.CreateEntity() };
	PTGN_DEFAULT_NAME(camera, "Camera");

	PTGN_ASSERT(
		!viewport_size.has_value() || viewport_size.value().IsPositive(),
		"Camera viewport size cannot be negative or zero"
	);

	SetTransform(camera, transform);

	auto& data{ camera.Add<impl::CameraData>() };
	data.viewport_space = viewport_space;

	if (viewport_size.has_value()) {
		camera.SetViewport(Viewport{ {}, viewport_size.value() }, viewport_space);
	}

	camera.SetRenderTarget(std::nullopt);

	return camera;
}

} // namespace ptgn