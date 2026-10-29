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
#include "runtime/ecs/entity_creation_context.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/group_stack.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event.h"

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

void UpdateCameraVisibilityEvents(Scene& scene) {
	if (!scene.IsRuntime() || scene.IsPaused()) {
		return;
	}
	for (auto [camera_entity, data] : scene.EntitiesWith<CameraData>()) {
		SceneCamera camera{ camera_entity };
		auto quad{ camera.GetWorldVertices() };
		std::vector<Entity> current;
		for (auto [entity, transform] : scene.EntitiesWith<Transform>()) {
			if (entity == camera_entity || !HasDraw(entity) || !IsVisible(entity) ||
				!camera.CanSee(entity) || !scene.ctx().chunks.ShouldRender(entity)) {
				continue;
			}
			V2_float point{ GetWorldPosition(entity) };
			float last_cross{ 0.0f };
			bool inside{ true };
			for (std::size_t i{ 0 }; i < quad.size(); ++i) {
				V2_float edge{ quad[(i + 1) % quad.size()] - quad[i] };
				float cross{ edge.Cross(point - quad[i]) };
				if (cross != 0.0f && last_cross != 0.0f && cross * last_cross < 0.0f) {
					inside = false;
					break;
				}
				if (cross != 0.0f) {
					last_cross = cross;
				}
			}
			if (inside) {
				current.push_back(entity);
				if (!std::ranges::contains(data.visible_last_frame, entity)) {
					PushEvent<event::CameraEntityEnteredView>(camera_entity, camera, entity);
					PushEvent<event::CameraEntityEnteredView>(entity, camera, entity);
				}
			}
		}
		for (Entity previous : data.visible_last_frame) {
			if (previous && !std::ranges::contains(current, previous)) {
				PushEvent<event::CameraEntityExitedView>(camera_entity, camera, previous);
				PushEvent<event::CameraEntityExitedView>(previous, camera, previous);
			}
		}
		data.visible_last_frame = std::move(current);
	}
}

void ApplyCameraBounds(SceneCamera camera) {
	V2_float attempted{ GetPosition(camera) };
	V2_float clamped{ ptgn::ApplyCameraBounds(camera, attempted) };
	if (clamped != attempted && camera.HasScene()) {
		PushEvent<event::CameraBoundsReached>(camera, attempted, clamped);
	}
	SetPosition(camera, clamped);
}

void RecalculateCameraViewProjection(SceneCamera camera) {
	auto& data{ camera.Get<impl::CameraData>() };

	V2_float view_size{ camera.GetLogicalViewport().size };

	if (!view_size.IsPositive()) {
		PTGN_WARN(
			"Camera has a zero or negative logical viewport size; "
			"clamping it to one logical unit"
		);

		view_size = Max(view_size, V2_float{ 1.0f, 1.0f });
	}

	if (data.pixel_rounding) {
		view_size = Max(view_size, V2_float{ 1.0f, 1.0f });
	}

	// TODO: Consider adding flip in the future.
	// V2_float flip_dir{ 1.0f, 1.0f };
	// Flip flip{ Flip::None };
	// switch (flip) {
	//  case Flip::None:       break;
	//  case Flip::Vertical:   flip_dir.y = -1.0f; break;
	//  case Flip::Horizontal: flip_dir.x = -1.0f; break;
	//  case Flip::Both:
	//      flip_dir.x = -1.0f;
	//      flip_dir.y = -1.0f;
	//      break;
	//  default: PTGN_ERROR("Unrecognized flip state");
	//}
	// view_size *= flip_dir;

	Transform camera_transform{ GetTransform(camera) };

	auto current_offsets{ GetOffset(camera) };

	camera_transform.Translate(current_offsets.position);

	camera_transform.Rotate(current_offsets.rotation);

	camera_transform.position = ptgn::ApplyCameraBounds(camera, camera_transform.position);

	data.view_projection =
		GetOrthographicViewProjection(camera_transform, view_size, data.pixel_rounding);
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
	auto clamped{ Clamp(new_zoom, 1000.0f * kEpsilon<float>, std::numeric_limits<float>::max()) };
	if (GetZoom() == clamped) {
		return *this;
	}
	PTGN_ASSERT(clamped.IsPositive(), "Cannot set negative or zero zoom");
	V2_float previous{ GetZoom() };
	SetScale(*this, 1.0f / clamped);
	if (HasScene()) {
		PushEvent<event::CameraZoomChanged>(*this, previous, clamped);
	}
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
			WithinRangeInclusive(viewport->position, 0.0f, 1.0f),
			"Viewport position must be between 0 and 1 in normalized viewport space"
		);

		PTGN_ASSERT(
			viewport->size.IsPositive(),
			"Viewport size must be positive in normalized viewport space"
		);

		PTGN_ASSERT(
			WithinRangeInclusive(viewport->size, 0.0f, 1.0f),
			"Viewport size must not exceed 1 in normalized viewport space"
		);

		PTGN_ASSERT(
			viewport->position.x + viewport->size.x <= 1.0f &&
				viewport->position.y + viewport->size.y <= 1.0f,
			"Normalized viewport must remain within the unit viewport"
		);
	} else if (viewport.has_value()) {
		PTGN_ASSERT(viewport->size.IsPositive(), "Viewport size must be positive");
	}

	auto& camera{ Get<impl::CameraData>() };

	camera.viewport_space = viewport_space;
	camera.raw_viewport	  = viewport;

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

SceneCamera& SceneCamera::SetRenderMasks(RenderMaskBits include, RenderMaskBits exclude) {
	auto& c	  = TryAdd<impl::CameraMask>();
	c.include = include;
	c.exclude = exclude;
	return *this;
}

SceneCamera& SceneCamera::SetIncludeRenderMask(RenderMaskBits include) {
	TryAdd<impl::CameraMask>().include = include;
	return *this;
}

SceneCamera& SceneCamera::SetExcludeRenderMask(RenderMaskBits exclude) {
	TryAdd<impl::CameraMask>().exclude = exclude;
	return *this;
}

SceneCamera& SceneCamera::AddIncludeRenderMaskBits(RenderMaskBits masks_to_add) {
	TryAdd<impl::CameraMask>().include |= masks_to_add;
	return *this;
}

SceneCamera& SceneCamera::RemoveIncludeRenderMaskBits(RenderMaskBits masks_to_remove) {
	TryAdd<impl::CameraMask>().include &= ~masks_to_remove;
	return *this;
}

SceneCamera& SceneCamera::AddExcludeRenderMaskBits(RenderMaskBits masks_to_add) {
	TryAdd<impl::CameraMask>().exclude |= masks_to_add;
	return *this;
}

SceneCamera& SceneCamera::RemoveExcludeRenderMaskBits(RenderMaskBits masks_to_remove) {
	TryAdd<impl::CameraMask>().exclude &= ~masks_to_remove;
	return *this;
}

SceneCamera& SceneCamera::ClearRenderMasks() {
	Remove<impl::CameraMask>();
	return *this;
}

RenderMaskBits SceneCamera::GetIncludeRenderMask() const {
	return GetOrDefault<impl::CameraMask>().include;
}

RenderMaskBits SceneCamera::GetExcludeRenderMask() const {
	return GetOrDefault<impl::CameraMask>().exclude;
}

bool SceneCamera::CanSee(Entity entity) const {
	auto entity_mask = GetRenderMask(entity);
	auto include	 = GetIncludeRenderMask();
	auto exclude	 = GetExcludeRenderMask();

	bool in_include = (entity_mask & include) != 0;
	bool in_exclude = (entity_mask & exclude) != 0;

	return (in_include && !in_exclude) || (IsUI(*this) && IsUI(entity));
}

SceneCamera& SceneCamera::SetRenderTarget(const std::optional<RenderTarget>& parent) {
	if (!parent.has_value() || parent.value() == GetScene().GetRenderTarget()) {
		Remove<impl::ParentRenderTarget>();
		return *this;
	}

	PTGN_ASSERT(
		parent.value(), "Cannot set camera parent render target to an invalid render target"
	);

	Add<impl::ParentRenderTarget>(parent.value().Get<UUID>());

	return *this;
}

RenderTarget SceneCamera::GetRenderTarget() const {
	const auto& scene{ GetScene() };

	if (!Has<impl::ParentRenderTarget>()) {
		return scene.GetRenderTarget();
	}

	RenderTarget parent{ scene.GetEntity(Get<impl::ParentRenderTarget>().render_target) };

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

void SetUI(Entity entity, bool ui_rendering) {
	if (!entity) {
		return;
	}

	if (ui_rendering) {
		entity.TryAdd<impl::UIRenderTag>();
		entity.TryAdd<impl::RenderMask>().bits = kRenderMaskNone;
	} else {
		entity.Remove<impl::UIRenderTag>();
		entity.TryAdd<impl::RenderMask>().bits = impl::RenderMask{}.bits;
	}

	// Group entities inherit the parent render mode unless explicitly overridden.
	if (entity.Has<Group>() && HasParent(entity) && !entity.Has<impl::RenderMaskOverride>() &&
		ui_rendering != IsUI(GetParent(entity))) {
		entity.TryAdd<impl::RenderMaskOverride>();
	}

	// Managed controls inside a group inherit that group unless overridden.
	GroupEntity layer{ GetGroup(entity) };
	if (layer && layer != entity && !entity.Has<impl::RenderMaskOverride>() &&
		ui_rendering != IsUI(layer)) {
		entity.TryAdd<impl::RenderMaskOverride>();
	}
}

bool IsUI(Entity entity) {
	if (!entity) {
		return false;
	}

	if (entity.Has<Group>() && HasParent(entity) && !entity.Has<impl::RenderMaskOverride>()) {
		return IsUI(GetParent(entity));
	}

	GroupEntity layer{ GetGroup(entity) };
	if (layer && layer != entity) {
		return entity.Has<impl::RenderMaskOverride>() ? entity.Has<impl::UIRenderTag>()
													  : IsUI(layer);
	}

	GroupStack stack{ GetGroupStack(entity) };
	if (layer && stack && layer == entity && !layer.Has<impl::RenderMaskOverride>()) {
		return IsUI(stack);
	}

	return entity.Has<impl::UIRenderTag>();
}

RenderMaskBits GetRenderMask(Entity entity) {
	if (!entity) {
		return kRenderMaskDefault;
	}

	if (entity.Has<Group>() && HasParent(entity) && !entity.Has<impl::RenderMaskOverride>()) {
		return GetRenderMask(GetParent(entity));
	}

	GroupEntity layer{ GetGroup(entity) };
	if (layer && layer != entity) {
		return entity.Has<impl::RenderMaskOverride>() ? entity.GetOrDefault<impl::RenderMask>().bits
													  : GetRenderMask(layer);
	}

	GroupStack stack{ GetGroupStack(entity) };
	if (layer && stack && layer == entity && !layer.Has<impl::RenderMaskOverride>()) {
		return GetRenderMask(stack);
	}

	return entity.GetOrDefault<impl::RenderMask>().bits;
}

void SetRenderMask(Entity entity, RenderMaskBits mask) {
	if (!entity) {
		return;
	}

	entity.TryAdd<impl::RenderMask>().bits = mask;
	if (GetGroup(entity)) {
		entity.TryAdd<impl::RenderMaskOverride>();
	}
}

void AddRenderMaskBits(Entity entity, RenderMaskBits masks_to_add) {
	SetRenderMask(entity, GetRenderMask(entity) | masks_to_add);
}

void RemoveRenderMaskBits(Entity entity, RenderMaskBits masks_to_remove) {
	SetRenderMask(entity, GetRenderMask(entity) & ~masks_to_remove);
}

void ClearRenderMask(Entity entity) {
	entity.Remove<impl::RenderMask>();
	entity.Remove<impl::RenderMaskOverride>();
}

void PrepareGroupEntityRenderMaskReparent(Entity entity, Entity new_parent) {
	if (!entity || !new_parent || entity.Has<impl::RenderMaskOverride>() || GetGroup(entity) ||
		!GetGroup(new_parent)) {
		return;
	}

	// Existing UI controls use None + UI rendering as their natural default.
	// Other entities use the default (bit zero) as their natural default.
	bool ui_rendering{ entity.Has<impl::UIRenderTag>() };
	RenderMaskBits mask{ entity.GetOrDefault<impl::RenderMask>().bits };
	RenderMaskBits default_mask{ ui_rendering ? kRenderMaskNone : kRenderMaskDefault };

	if (mask != default_mask) {
		entity.TryAdd<impl::RenderMaskOverride>();
	}
}

bool HasAnyRenderMaskBits(Entity entity, RenderMaskBits test) {
	return (GetRenderMask(entity) & test) != 0;
}

bool HasAllRenderMaskBits(Entity entity, RenderMaskBits test) {
	auto m{ GetRenderMask(entity) };
	return (m & test) == test;
}

SceneCamera CreateCamera(
	EntityCreationContext context, Transform transform, std::optional<V2_float> viewport_size,
	ViewportSpace viewport_space
) {
	SceneCamera camera{ context.CreateEntity() };

	PTGN_ASSERT(
		!viewport_size.has_value() || viewport_size.value().IsPositive(),
		"Camera viewport size cannot be negative or zero"
	);

	camera.Add<Tag>("Camera");
	camera.Add<Transform>(transform);

	auto& data{ camera.Add<impl::CameraData>() };
	data.viewport_space = viewport_space;

	if (viewport_size.has_value()) {
		camera.SetViewport(Viewport{ {}, viewport_size.value() }, viewport_space);
	}

	// An independent camera has no default scene render target to bind.
	// On instantiation, the absence of ParentRenderTarget selects the scene default.
	return camera;
}

} // namespace ptgn
