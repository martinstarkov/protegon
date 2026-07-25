#include "panels/viewport.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "core/assert.h"
#include "core/editor.h"
#include "core/editor_context.h"
#include "core/editor_state.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "panels/scene_hierarchy.h"
#include "platform/window.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "tools/debug/stats.h"

namespace ptgn::editor {

namespace {

constexpr Color kCameraOutlineColor{ color::Blue };
constexpr Color kFixedCameraOutlineColor{ color::Red };

constexpr GizmoOccurrenceId kDirectOccurrenceId{ 0 };
constexpr std::size_t kMaximumRenderPathDepth{ 16 };

constexpr V2_float kGizmoAxisLengthPixels{ 70.0f, 70.0f };
constexpr float kGizmoHandleRadiusPixels{ 8.0f };
constexpr V2_float kGizmoCenterHalfSizePixels{ 7.0f, 7.0f };
constexpr float kGizmoRotateRadiusPixels{ 48.0f };
constexpr float kGizmoRotateThicknessPixels{ 8.0f };
constexpr float kGizmoAxisHitThicknessPixels{ 6.0f };
constexpr float kScaleDragPixels{ 100.0f };
constexpr float kMinimumScale{ 0.01f };

struct EntityRenderPath {
	GizmoOccurrenceId id;
	std::vector<Entity> cameras;
};

struct GizmoProjection2D {
	V2_float world_anchor;
	V2_float screen_anchor;
	V2_float screen_basis_x;
	V2_float screen_basis_y;

	[[nodiscard]] V2_float Project(V2_float world_position) const {
		auto local{ world_position - world_anchor };
		return screen_anchor + screen_basis_x * local.x + screen_basis_y * local.y;
	}

	[[nodiscard]] std::optional<V2_float> Unproject(V2_float screen_position) const {
		auto delta{ screen_position - screen_anchor };

		float determinant{ screen_basis_x.x * screen_basis_y.y -
						   screen_basis_x.y * screen_basis_y.x };

		if (std::abs(determinant) <= 1e-6f) {
			return std::nullopt;
		}

		V2_float local{
			(delta.x * screen_basis_y.y - delta.y * screen_basis_y.x) / determinant,
			(screen_basis_x.x * delta.y - screen_basis_x.y * delta.x) / determinant,
		};

		return world_anchor + local;
	}
};

struct GizmoInstance {
	GizmoOccurrenceId id;
	GizmoProjection2D projection;
	std::size_t draw_order{};
};

struct GizmoGeometry {
	V2_float pivot_screen;
	V2_float axis_x_screen;
	V2_float axis_y_screen;
	V2_float world_axis_x;
	V2_float world_axis_y;
};

struct GizmoHit {
	GizmoOccurrenceId occurrence;
	GizmoHandle handle{ GizmoHandle::None };
	float distance{ std::numeric_limits<float>::max() };
	std::size_t draw_order{};
};

void ApplyGizmoDeltaToTransform(
	Transform& transform, GizmoHandle handle, const Transform& before, const Transform& after
) {
	switch (handle) {
		case GizmoHandle::MoveCenter:
		case GizmoHandle::MoveX:
		case GizmoHandle::MoveY:	  {
			transform.position += after.position - before.position;
			break;
		}

		case GizmoHandle::Rotate: {
			transform.rotation =
				Radians{ transform.rotation.value + after.rotation.value - before.rotation.value };
			break;
		}

		case GizmoHandle::ScaleX:
		case GizmoHandle::ScaleY:
		case GizmoHandle::ScaleUniform: {
			PTGN_ASSERT(
				std::abs(before.scale.x) > 1e-6f && std::abs(before.scale.y) > 1e-6f,
				"Cannot calculate gizmo scale delta from a zero scale"
			);

			V2_float scale_factor{
				after.scale.x / before.scale.x,
				after.scale.y / before.scale.y,
			};

			transform.scale *= scale_factor;
			transform.ClampScale();
			break;
		}

		default: break;
	}
}

template <typename TVisuals>
bool ApplyGizmoDeltaToVisualTransforms(
	TVisuals& visuals, GizmoHandle handle, const Transform& before, const Transform& after
) {
	bool changed{ false };

	for (auto& visual : visuals.states) {
		if (!visual.transform.has_value()) {
			continue;
		}

		ApplyGizmoDeltaToTransform(visual.transform.value(), handle, before, after);
		changed = true;
	}

	return changed;
}

void MarkParentButtonDirty(Entity entity, impl::ButtonDirty dirty) {
	Entity parent{ GetParent(entity) };

	if (!parent || !parent.Has<impl::ButtonData>()) {
		return;
	}

	parent.Get<impl::ButtonData>().dirty |= dirty;
}

bool ApplyGizmoDeltaToButtonVisualTransforms(
	Entity entity, GizmoHandle handle, const Transform& before, const Transform& after
) {
	bool changed{ false };

	if (auto visuals{ entity.TryGet<ButtonBackgroundVisuals>() }) {
		if (ApplyGizmoDeltaToVisualTransforms(*visuals, handle, before, after)) {
			MarkParentButtonDirty(entity, impl::ButtonDirty::Background);
			changed = true;
		}
	}

	if (auto visuals{ entity.TryGet<ButtonBorderVisuals>() }) {
		if (ApplyGizmoDeltaToVisualTransforms(*visuals, handle, before, after)) {
			MarkParentButtonDirty(entity, impl::ButtonDirty::Border);
			changed = true;
		}
	}

	if (auto visuals{ entity.TryGet<ButtonTextVisuals>() }) {
		if (ApplyGizmoDeltaToVisualTransforms(*visuals, handle, before, after)) {
			MarkParentButtonDirty(entity, impl::ButtonDirty::Text);

			if (entity.Has<TextLayout>()) {
				entity.Get<TextLayout>().dirty = true;
			}

			changed = true;
		}
	}

	if (auto visuals{ entity.TryGet<ButtonSpriteVisuals>() }) {
		if (ApplyGizmoDeltaToVisualTransforms(*visuals, handle, before, after)) {
			MarkParentButtonDirty(entity, impl::ButtonDirty::Sprite);
			changed = true;
		}
	}

	return changed;
}

[[maybe_unused]] ImVec2 ToImGui(V2_float v) {
	return { v.x, v.y };
}

[[maybe_unused]] ImU32 ToImGui(Color c) {
	return c.ToUint32(ColorPacking::ABGR);
}

[[maybe_unused]] V2_float FromImGui(ImVec2 v) {
	return { v.x, v.y };
}

[[maybe_unused]] Color FromImGui(ImU32 color) {
	return Color{ color };
}

V2_float PresentationToScreen(V2_float presentation_point, Viewport presentation_viewport) {
	return presentation_point + presentation_viewport.GetCenter();
}

V2_float WorldToScreen(
	V2_float world_point, const FrameContext& frame_context, Viewport presentation_viewport,
	Frame from
) {
	auto presentation_point{ ConvertPoint(world_point, from, Frame::Presentation, frame_context) };

	return PresentationToScreen(presentation_point, presentation_viewport);
}

V2_float ScreenToPresentation(V2_float screen_point, Viewport presentation_viewport) {
	return screen_point - presentation_viewport.GetCenter();
}

V2_float ScreenToWorld(
	V2_float screen_point, const FrameContext& frame_context, Viewport presentation_viewport,
	Frame to
) {
	auto presentation_point{ ScreenToPresentation(screen_point, presentation_viewport) };

	return ConvertPoint(presentation_point, Frame::Presentation, to, frame_context);
}

std::optional<V2_int> WorldToRenderTargetPixel(
	Entity render_target_entity, V2_float world_position
) {
	if (!render_target_entity.Has<impl::FramebufferObject>()) {
		return std::nullopt;
	}

	RenderTarget render_target{ render_target_entity };

	auto size{ render_target.GetSize() };

	if (!size.IsPositive()) {
		return std::nullopt;
	}

	auto draw_transform{ GetDrawTransform(render_target_entity) };

	auto local_position{ draw_transform.ApplyInverse(world_position) };

	Rect local_rect{ V2_float{ size }, render_target_entity.GetOrDefault<Origin>() };

	if (local_position.x < local_rect.min.x || local_position.x >= local_rect.max.x ||
		local_position.y < local_rect.min.y || local_position.y >= local_rect.max.y) {
		return std::nullopt;
	}

	auto uv{ (local_position - local_rect.min) / local_rect.GetSize() };

	V2_int pixel{
		static_cast<int>(uv.x * static_cast<float>(size.x)),
		static_cast<int>(uv.y * static_cast<float>(size.y)),
	};

	return Clamp(pixel, V2_int{ 0, 0 }, size - V2_int{ 1, 1 });
}

std::optional<V2_int> ScreenToFramebufferPixel(
	V2_float screen_position, Viewport image_viewport, V2_int framebuffer_size
) {
	auto image_min{ image_viewport.position };
	auto image_max{ image_viewport.position + image_viewport.size };

	if (screen_position.x < image_min.x || screen_position.x >= image_max.x ||
		screen_position.y < image_min.y || screen_position.y >= image_max.y) {
		return std::nullopt;
	}

	auto local{ screen_position - image_min };

	V2_float uv{
		local.x / image_viewport.size.x,
		local.y / image_viewport.size.y,
	};

	V2_int pixel{
		static_cast<int>(uv.x * static_cast<float>(framebuffer_size.x)),
		static_cast<int>(uv.y * static_cast<float>(framebuffer_size.y)),
	};

	return Clamp(pixel, V2_int{ 0, 0 }, framebuffer_size - V2_int{ 1, 1 });
}

Entity ResolveRenderTargetPick(
	Scene& scene, impl::RendererAccessor& renderer, Entity outer_entity, V2_float world_position
) {
	if (!outer_entity || !outer_entity.Has<impl::FramebufferObject>()) {
		return outer_entity;
	}

	RenderTarget render_target{ outer_entity };

	auto framebuffer{
		static_cast<impl::FramebufferId>(render_target.Get<impl::FramebufferObject>())
	};

	if (!renderer.IsEntityPickingEnabled(framebuffer)) {
		return outer_entity;
	}

	auto pixel{ WorldToRenderTargetPixel(outer_entity, world_position) };

	if (!pixel.has_value()) {
		return outer_entity;
	}

	auto nested_id{ renderer.ReadEntityId(framebuffer, pixel.value()) };

	if (!nested_id.has_value() || nested_id.value() == impl::kNoEntityId) {
		return outer_entity;
	}

	PTGN_ASSERT(
		nested_id.value() >= 0, "Render target returned an invalid entity ID: ", nested_id.value()
	);

	auto nested_entity{ scene.GetEntity(UUID{ nested_id.value() }) };

	if (!nested_entity) {
		return outer_entity;
	}

	// Protect against a render target rendering itself.
	if (nested_entity == outer_entity) {
		return outer_entity;
	}

	return nested_entity;
}

void UpdateEditorCamera(EditorCamera& editor_camera) {
	const auto& io{ ImGui::GetIO() };

	if (!ImGui::IsWindowHovered()) {
		return;
	}

	auto& camera{ editor_camera.camera };

	PTGN_ASSERT(camera.transform.scale.IsPositive());

	auto zoom{ 1.0f / camera.transform.scale };

	// Middle mouse held.
	if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		auto delta{ io.MouseDelta };

		// Divide by zoom so panning has a consistent screen-space speed.
		camera.transform.position = camera.transform.position - FromImGui(delta) / zoom;
	}

	// Mouse wheel zoom.
	if (io.MouseWheel != 0.0f) {
		float zoom_factor{ std::pow(1.1f, io.MouseWheel) };

		zoom = Clamp(zoom * zoom_factor, 0.1f, 10.0f);
		PTGN_ASSERT(zoom.IsPositive());
		camera.transform.scale = 1.0f / zoom;
	}
}

std::optional<V2_float> RenderTargetPixelToWorld(Entity render_target_entity, V2_float pixel) {
	if (!render_target_entity.Has<impl::FramebufferObject>()) {
		return std::nullopt;
	}

	RenderTarget render_target{ render_target_entity };
	auto size{ render_target.GetSize() };

	if (!size.IsPositive()) {
		return std::nullopt;
	}

	Rect local_rect{ V2_float{ size }, render_target_entity.GetOrDefault<Origin>() };

	auto uv{ pixel / V2_float{ size } };
	auto local_position{ local_rect.min + uv * local_rect.GetSize() };

	return GetDrawTransform(render_target_entity).Apply(local_position);
}

std::optional<V2_float> ProjectWorldToCameraTarget(SceneCamera camera, V2_float world_position) {
	auto clip{ camera.GetViewProjection() *
			   V4_float{ world_position.x, world_position.y, 0.0f, 1.0f } };

	if (std::abs(clip.w) <= 1e-6f) {
		return std::nullopt;
	}

	float inverse_w{ 1.0f / clip.w };
	V2_float ndc{ clip.x * inverse_w, clip.y * inverse_w };

	auto viewport{ camera.GetDisplayViewport() };

	return V2_float{
		viewport.position.x + (ndc.x * 0.5f + 0.5f) * viewport.size.x,
		viewport.position.y + (-ndc.y * 0.5f + 0.5f) * viewport.size.y,
	};
}

bool ContainsEntity(const std::vector<Entity>& entities, Entity entity) {
	return std::find(entities.begin(), entities.end(), entity) != entities.end();
}

std::vector<Entity> GetCustomTargetCameras(Scene& scene) {
	std::vector<Entity> cameras;

	for (auto [entity, _camera] : scene.EntitiesWith<impl::CameraData>()) {
		SceneCamera camera{ entity };

		if (camera.GetRenderTarget() != scene.GetRenderTarget()) {
			cameras.push_back(entity);
		}
	}

	std::stable_sort(cameras.begin(), cameras.end(), [](Entity lhs, Entity rhs) {
		auto lhs_depth{ GetDepth(lhs) };
		auto rhs_depth{ GetDepth(rhs) };

		if (lhs_depth != rhs_depth) {
			return lhs_depth < rhs_depth;
		}

		return lhs.WasCreatedBefore(rhs);
	});

	return cameras;
}

bool IsRenderedByEditorCamera(Scene& scene, Entity entity) {
	if (!IsVisible(entity)) {
		return false;
	}

	auto entity_mask{ GetMask(entity) };
	auto include{ scene.ctx().camera.GetIncludeMask() };
	auto exclude{ scene.ctx().camera.GetExcludeMask() };

	bool in_include{ (entity_mask & include) != 0 };
	bool in_exclude{ (entity_mask & exclude) != 0 };

	return (in_include && !in_exclude) || IsUI(entity);
}

bool IsRenderedByCamera(SceneCamera camera, Entity entity) {
	return IsVisible(entity) && camera.CanSee(entity);
}

GizmoOccurrenceId MakeOccurrenceId(const std::vector<Entity>& camera_path) {
	constexpr std::uint64_t kOffsetBasis{ 1469598103934665603ull };
	constexpr std::uint64_t kPrime{ 1099511628211ull };

	std::uint64_t hash{ kOffsetBasis };

	for (auto camera : camera_path) {
		hash ^= static_cast<std::uint64_t>(camera.Get<UUID>());
		hash *= kPrime;
	}

	// Zero is reserved for the direct occurrence.
	if (hash == kDirectOccurrenceId.value) {
		hash = 1;
	}

	return GizmoOccurrenceId{ hash };
}

void AppendCustomRenderPaths(
	Scene& scene, Entity rendered_entity, std::vector<Entity>& camera_path,
	std::vector<Entity>& target_path, std::vector<EntityRenderPath>& paths,
	std::size_t recursion_depth
) {
	if (recursion_depth >= kMaximumRenderPathDepth) {
		return;
	}

	for (auto camera_entity : GetCustomTargetCameras(scene)) {
		SceneCamera camera{ camera_entity };
		auto render_target{ camera.GetRenderTarget() };

		if (ContainsEntity(camera_path, camera_entity) ||
			ContainsEntity(target_path, render_target) || render_target == rendered_entity) {
			continue;
		}

		if (!IsRenderedByCamera(camera, rendered_entity)) {
			continue;
		}

		camera_path.push_back(camera_entity);
		target_path.push_back(render_target);

		if (IsRenderedByEditorCamera(scene, render_target)) {
			paths.push_back(
				EntityRenderPath{
					.id{ MakeOccurrenceId(camera_path) },
					.cameras{ camera_path },
				}
			);
		}

		AppendCustomRenderPaths(
			scene, render_target, camera_path, target_path, paths, recursion_depth + 1
		);

		target_path.pop_back();
		camera_path.pop_back();
	}
}

std::vector<EntityRenderPath> BuildEntityRenderPaths(Scene& scene, Entity entity) {
	std::vector<EntityRenderPath> paths;

	if (IsRenderedByEditorCamera(scene, entity)) {
		paths.push_back(EntityRenderPath{ .id{ kDirectOccurrenceId } });
	}

	std::vector<Entity> camera_path;
	std::vector<Entity> target_path;

	AppendCustomRenderPaths(scene, entity, camera_path, target_path, paths, 0);

	// Keep hierarchy-selected entities editable even if they are hidden or masked from every
	// camera.
	if (paths.empty()) {
		paths.push_back(EntityRenderPath{ .id{ kDirectOccurrenceId } });
	}

	return paths;
}

std::optional<V2_float> ProjectWorldPointThroughPath(
	V2_float world_position, const EntityRenderPath& path, Frame direct_frame,
	const FrameContext& frame_context, Viewport presentation_viewport
) {
	if (path.cameras.empty()) {
		return WorldToScreen(world_position, frame_context, presentation_viewport, direct_frame);
	}

	V2_float point{ world_position };

	for (auto camera_entity : path.cameras) {
		SceneCamera camera{ camera_entity };

		auto target_pixel{ ProjectWorldToCameraTarget(camera, point) };
		if (!target_pixel.has_value()) {
			return std::nullopt;
		}

		auto parent_world{
			RenderTargetPixelToWorld(camera.GetRenderTarget(), target_pixel.value())
		};
		if (!parent_world.has_value()) {
			return std::nullopt;
		}

		point = parent_world.value();
	}

	return WorldToScreen(point, frame_context, presentation_viewport, Frame::World);
}

std::optional<GizmoProjection2D> BuildGizmoProjection(
	const EntityRenderPath& path, V2_float world_anchor, Frame direct_frame,
	const FrameContext& frame_context, Viewport presentation_viewport
) {
	auto screen_anchor{ ProjectWorldPointThroughPath(
		world_anchor, path, direct_frame, frame_context, presentation_viewport
	) };

	auto screen_x{ ProjectWorldPointThroughPath(
		world_anchor + V2_float{ 1.0f, 0.0f }, path, direct_frame, frame_context,
		presentation_viewport
	) };

	auto screen_y{ ProjectWorldPointThroughPath(
		world_anchor + V2_float{ 0.0f, 1.0f }, path, direct_frame, frame_context,
		presentation_viewport
	) };

	if (!screen_anchor.has_value() || !screen_x.has_value() || !screen_y.has_value()) {
		return std::nullopt;
	}

	GizmoProjection2D projection{
		.world_anchor{ world_anchor },
		.screen_anchor{ screen_anchor.value() },
		.screen_basis_x{ screen_x.value() - screen_anchor.value() },
		.screen_basis_y{ screen_y.value() - screen_anchor.value() },
	};

	float determinant{ projection.screen_basis_x.x * projection.screen_basis_y.y -
					   projection.screen_basis_x.y * projection.screen_basis_y.x };

	if (std::abs(determinant) <= 1e-6f) {
		return std::nullopt;
	}

	return projection;
}

std::vector<GizmoInstance> BuildGizmoInstances(
	const std::vector<EntityRenderPath>& paths, V2_float world_anchor, Frame direct_frame,
	const FrameContext& frame_context, Viewport presentation_viewport
) {
	std::vector<GizmoInstance> instances;
	instances.reserve(paths.size());

	for (std::size_t i{ 0 }; i < paths.size(); ++i) {
		auto projection{ BuildGizmoProjection(
			paths[i], world_anchor, direct_frame, frame_context, presentation_viewport
		) };

		if (!projection.has_value()) {
			continue;
		}

		instances.push_back(
			GizmoInstance{
				.id{ paths[i].id },
				.projection{ projection.value() },
				.draw_order = i,
			}
		);
	}

	return instances;
}

float SignedAngle(V2_float from, V2_float to) {
	float cross{ from.x * to.y - from.y * to.x };
	float dot{ Dot(from, to) };
	return std::atan2(cross, dot);
}

V2_float GizmoLocalToScreen(
	V2_float pivot_screen, V2_float axis_x_screen, V2_float axis_y_screen, V2_float local
) {
	return pivot_screen + axis_x_screen * local.x + axis_y_screen * local.y;
}

float DistanceToSegment(V2_float point, V2_float a, V2_float b) {
	auto ab{ b - a };
	float ab_length_squared{ Dot(ab, ab) };

	if (ab_length_squared <= 1e-6f) {
		return Length(point - a);
	}

	float t{ Dot(point - a, ab) / ab_length_squared };
	t = Clamp01(t);

	return Length(point - (a + ab * t));
}

std::optional<V2_float> ScreenDeltaToGizmoLocal(
	V2_float delta, V2_float axis_x_screen, V2_float axis_y_screen
) {
	float determinant{ axis_x_screen.x * axis_y_screen.y - axis_x_screen.y * axis_y_screen.x };

	if (std::abs(determinant) <= 1e-6f) {
		return std::nullopt;
	}

	return V2_float{
		(delta.x * axis_y_screen.y - delta.y * axis_y_screen.x) / determinant,
		(axis_x_screen.x * delta.y - axis_x_screen.y * delta.x) / determinant,
	};
}

std::optional<GizmoGeometry> BuildGizmoGeometry(
	const GizmoInstance& instance, const Transform& transform, bool use_local_orientation
) {
	float gizmo_angle{ use_local_orientation ? transform.rotation.value : 0.0f };

	V2_float world_axis_x{
		std::cos(gizmo_angle),
		std::sin(gizmo_angle),
	};

	V2_float world_axis_y{
		-std::sin(gizmo_angle),
		std::cos(gizmo_angle),
	};

	auto pivot_screen{ instance.projection.Project(transform.position) };

	auto axis_x_screen{ instance.projection.Project(transform.position + world_axis_x) -
						pivot_screen };

	auto axis_y_screen{ instance.projection.Project(transform.position + world_axis_y) -
						pivot_screen };

	if (Length(axis_x_screen) <= 1e-6f || Length(axis_y_screen) <= 1e-6f) {
		return std::nullopt;
	}

	axis_x_screen = Normalize(axis_x_screen);
	axis_y_screen = Normalize(axis_y_screen);

	return GizmoGeometry{
		.pivot_screen{ pivot_screen },
		.axis_x_screen{ axis_x_screen },
		.axis_y_screen{ axis_y_screen },
		.world_axis_x{ world_axis_x },
		.world_axis_y{ world_axis_y },
	};
}

std::optional<GizmoHit> HitTestGizmo(
	const GizmoInstance& instance, const Transform& transform, GizmoTool tool,
	V2_float mouse_screen, bool use_local_orientation
) {
	auto geometry{ BuildGizmoGeometry(instance, transform, use_local_orientation) };
	if (!geometry.has_value()) {
		return std::nullopt;
	}

	const auto& g{ geometry.value() };
	auto mouse_local{
		ScreenDeltaToGizmoLocal(mouse_screen - g.pivot_screen, g.axis_x_screen, g.axis_y_screen)
	};

	if (!mouse_local.has_value()) {
		return std::nullopt;
	}

	auto make_hit = [&](GizmoHandle handle, float distance) {
		return GizmoHit{
			.occurrence{ instance.id },
			.handle		= handle,
			.distance	= distance,
			.draw_order = instance.draw_order,
		};
	};

	bool inside_center{ std::abs(mouse_local->x) <= kGizmoCenterHalfSizePixels.x &&
						std::abs(mouse_local->y) <= kGizmoCenterHalfSizePixels.y };

	if (tool == GizmoTool::Translate) {
		if (inside_center) {
			return make_hit(GizmoHandle::MoveCenter, Length(mouse_screen - g.pivot_screen));
		}

		auto x_end{ g.pivot_screen + g.axis_x_screen * kGizmoAxisLengthPixels.x };
		auto y_end{ g.pivot_screen + g.axis_y_screen * kGizmoAxisLengthPixels.y };

		float distance_x{ DistanceToSegment(mouse_screen, g.pivot_screen, x_end) };
		float distance_y{ DistanceToSegment(mouse_screen, g.pivot_screen, y_end) };

		if (distance_x < kGizmoAxisHitThicknessPixels && distance_x <= distance_y) {
			return make_hit(GizmoHandle::MoveX, distance_x);
		}

		if (distance_y < kGizmoAxisHitThicknessPixels) {
			return make_hit(GizmoHandle::MoveY, distance_y);
		}
	} else if (tool == GizmoTool::Rotate) {
		float radius_distance{
			std::abs(Length(mouse_screen - g.pivot_screen) - kGizmoRotateRadiusPixels)
		};

		if (radius_distance <= kGizmoRotateThicknessPixels * 0.5f) {
			return make_hit(GizmoHandle::Rotate, radius_distance);
		}
	} else if (tool == GizmoTool::Scale) {
		if (inside_center) {
			return make_hit(GizmoHandle::ScaleUniform, Length(mouse_screen - g.pivot_screen));
		}

		auto x_end{ g.pivot_screen + g.axis_x_screen * kGizmoAxisLengthPixels.x };
		auto y_end{ g.pivot_screen + g.axis_y_screen * kGizmoAxisLengthPixels.y };

		float distance_x{ Distance(mouse_screen, x_end) };
		float distance_y{ Distance(mouse_screen, y_end) };

		if (distance_x <= kGizmoHandleRadiusPixels && distance_x <= distance_y) {
			return make_hit(GizmoHandle::ScaleX, distance_x);
		}

		if (distance_y <= kGizmoHandleRadiusPixels) {
			return make_hit(GizmoHandle::ScaleY, distance_y);
		}
	}

	return std::nullopt;
}

std::optional<GizmoHit> FindBestGizmoHit(
	const std::vector<GizmoInstance>& instances, const Transform& transform, GizmoTool tool,
	V2_float mouse_screen, bool use_local_orientation
) {
	std::optional<GizmoHit> best;

	for (const auto& instance : instances) {
		auto hit{ HitTestGizmo(instance, transform, tool, mouse_screen, use_local_orientation) };

		if (!hit.has_value()) {
			continue;
		}

		if (!best.has_value() || hit->distance < best->distance - 1e-4f ||
			(std::abs(hit->distance - best->distance) <= 1e-4f &&
			 hit->draw_order >= best->draw_order)) {
			best = hit;
		}
	}

	return best;
}

const GizmoInstance* FindGizmoInstance(
	const std::vector<GizmoInstance>& instances, GizmoOccurrenceId occurrence
) {
	auto it{ std::find_if(instances.begin(), instances.end(), [&](const GizmoInstance& instance) {
		return instance.id == occurrence;
	}) };

	return it != instances.end() ? &*it : nullptr;
}

void ResetGizmoInteraction(GizmoState& gizmo) {
	gizmo.hot = GizmoHandle::None;
	gizmo.hot_occurrence.reset();
	gizmo.active = GizmoHandle::None;
	gizmo.active_occurrence.reset();
}

void BeginGizmoDrag(
	GizmoState& gizmo, const GizmoInstance& instance, const GizmoGeometry& geometry,
	Transform& transform, V2_float mouse_screen
) {
	auto mouse_world{ instance.projection.Unproject(mouse_screen) };
	if (!mouse_world.has_value()) {
		return;
	}

	gizmo.active				   = gizmo.hot;
	gizmo.active_occurrence		   = instance.id;
	gizmo.drag_start_mouse_world   = mouse_world.value();
	gizmo.drag_start_mouse_screen  = mouse_screen;
	gizmo.drag_start_pivot_screen  = geometry.pivot_screen;
	gizmo.drag_start_position	   = transform.position;
	gizmo.drag_start_scale		   = transform.scale;
	gizmo.drag_start_rotation	   = transform.rotation;
	gizmo.drag_start_axis_x_world  = geometry.world_axis_x;
	gizmo.drag_start_axis_y_world  = geometry.world_axis_y;
	gizmo.drag_start_axis_x_screen = geometry.axis_x_screen;
	gizmo.drag_start_axis_y_screen = geometry.axis_y_screen;
}

void UpdateActiveGizmo(
	GizmoState& gizmo, const GizmoInstance& instance, Transform& transform, V2_float mouse_screen
) {
	auto current_mouse_world{ instance.projection.Unproject(mouse_screen) };
	if (!current_mouse_world.has_value()) {
		return;
	}

	V2_float screen_delta{ mouse_screen - gizmo.drag_start_mouse_screen };
	V2_float world_delta{ current_mouse_world.value() - gizmo.drag_start_mouse_world };

	auto drag_axis_x_world{ gizmo.drag_start_axis_x_world };
	auto drag_axis_y_world{ gizmo.drag_start_axis_y_world };

	switch (gizmo.active) {
		case GizmoHandle::MoveCenter: {
			transform.position = gizmo.drag_start_position + world_delta;
			break;
		}

		case GizmoHandle::MoveX: {
			float amount{ Dot(world_delta, drag_axis_x_world) };
			transform.position = gizmo.drag_start_position + drag_axis_x_world * amount;
			break;
		}

		case GizmoHandle::MoveY: {
			float amount{ Dot(world_delta, drag_axis_y_world) };
			transform.position = gizmo.drag_start_position + drag_axis_y_world * amount;
			break;
		}

		case GizmoHandle::Rotate: {
			V2_float start_direction{ gizmo.drag_start_mouse_world - gizmo.drag_start_position };
			V2_float current_direction{ current_mouse_world.value() - gizmo.drag_start_position };

			if (Length(start_direction) > 1e-6f && Length(current_direction) > 1e-6f) {
				start_direction	  = Normalize(start_direction);
				current_direction = Normalize(current_direction);

				float delta{ SignedAngle(start_direction, current_direction) };
				transform.rotation = Radians{ gizmo.drag_start_rotation.value + delta };
			}
			break;
		}

		case GizmoHandle::ScaleX: {
			float delta_pixels{ Dot(screen_delta, gizmo.drag_start_axis_x_screen) };
			float factor{ std::max(0.01f, 1.0f + delta_pixels / kScaleDragPixels) };

			auto scale{ gizmo.drag_start_scale };
			scale.x = std::max(kMinimumScale, gizmo.drag_start_scale.x * factor);

			transform.scale = scale;
			transform.ClampScale();
			break;
		}

		case GizmoHandle::ScaleY: {
			float delta_pixels{ Dot(screen_delta, gizmo.drag_start_axis_y_screen) };
			float factor{ std::max(0.01f, 1.0f + delta_pixels / kScaleDragPixels) };

			auto scale{ gizmo.drag_start_scale };
			scale.y = std::max(kMinimumScale, gizmo.drag_start_scale.y * factor);

			transform.scale = scale;
			transform.ClampScale();
			break;
		}

		case GizmoHandle::ScaleUniform: {
			auto uniform_direction{ gizmo.drag_start_axis_x_screen +
									gizmo.drag_start_axis_y_screen };

			if (Length(uniform_direction) > 1e-6f) {
				uniform_direction = Normalize(uniform_direction);

				float delta_pixels{ Dot(screen_delta, uniform_direction) };
				float factor{ std::max(0.01f, 1.0f + delta_pixels / kScaleDragPixels) };

				auto scale{ gizmo.drag_start_scale * factor };
				scale.x = std::max(kMinimumScale, scale.x);
				scale.y = std::max(kMinimumScale, scale.y);

				transform.scale = scale;
				transform.ClampScale();
			}
			break;
		}

		default: break;
	}
}

void DrawGizmoInstance(
	ImDrawList* draw_list, const GizmoState& gizmo, const GizmoInstance& instance,
	const Transform& transform, bool use_local_orientation
) {
	auto geometry{ BuildGizmoGeometry(instance, transform, use_local_orientation) };
	if (!geometry.has_value()) {
		return;
	}

	const auto& g{ geometry.value() };

	auto is_hot = [&](GizmoHandle handle) {
		return gizmo.hot == handle && gizmo.hot_occurrence.has_value() &&
			   gizmo.hot_occurrence.value() == instance.id;
	};

	auto is_active = [&](GizmoHandle handle) {
		return gizmo.active == handle && gizmo.active_occurrence.has_value() &&
			   gizmo.active_occurrence.value() == instance.id;
	};

	auto col_x{ ToImGui(Color{ 220, 60, 60, 255 }) };
	auto col_y{ ToImGui(Color{ 60, 220, 60, 255 }) };
	auto col_center{ ToImGui(Color{ 230, 200, 80, 255 }) };
	auto col_rotate{ ToImGui(Color{ 80, 160, 255, 255 }) };
	auto col_hot{ ToImGui(Color{ 255, 255, 255, 255 }) };

	auto x_end_screen{ g.pivot_screen + g.axis_x_screen * kGizmoAxisLengthPixels.x };
	auto y_end_screen{ g.pivot_screen + g.axis_y_screen * kGizmoAxisLengthPixels.y };

	if (gizmo.tool == GizmoTool::Translate) {
		draw_list->AddLine(
			ToImGui(g.pivot_screen), ToImGui(x_end_screen),
			is_hot(GizmoHandle::MoveX) || is_active(GizmoHandle::MoveX) ? col_hot : col_x, 2.0f
		);

		draw_list->AddLine(
			ToImGui(g.pivot_screen), ToImGui(y_end_screen),
			is_hot(GizmoHandle::MoveY) || is_active(GizmoHandle::MoveY) ? col_hot : col_y, 2.0f
		);

		auto h{ kGizmoCenterHalfSizePixels };

		auto c0{ GizmoLocalToScreen(
			g.pivot_screen, g.axis_x_screen, g.axis_y_screen, V2_float{ -h.x, -h.y }
		) };
		auto c1{ GizmoLocalToScreen(
			g.pivot_screen, g.axis_x_screen, g.axis_y_screen, V2_float{ h.x, -h.y }
		) };
		auto c2{ GizmoLocalToScreen(
			g.pivot_screen, g.axis_x_screen, g.axis_y_screen, V2_float{ h.x, h.y }
		) };
		auto c3{ GizmoLocalToScreen(
			g.pivot_screen, g.axis_x_screen, g.axis_y_screen, V2_float{ -h.x, h.y }
		) };

		draw_list->AddQuadFilled(
			ToImGui(c0), ToImGui(c1), ToImGui(c2), ToImGui(c3),
			is_hot(GizmoHandle::MoveCenter) || is_active(GizmoHandle::MoveCenter) ? col_hot
																				  : col_center
		);
	} else if (gizmo.tool == GizmoTool::Rotate) {
		draw_list->AddCircle(
			ToImGui(g.pivot_screen), kGizmoRotateRadiusPixels,
			is_hot(GizmoHandle::Rotate) || is_active(GizmoHandle::Rotate) ? col_hot : col_rotate,
			64, kGizmoRotateThicknessPixels
		);
	} else if (gizmo.tool == GizmoTool::Scale) {
		draw_list->AddLine(ToImGui(g.pivot_screen), ToImGui(x_end_screen), col_x, 2.0f);
		draw_list->AddLine(ToImGui(g.pivot_screen), ToImGui(y_end_screen), col_y, 2.0f);

		draw_list->AddCircleFilled(
			ToImGui(x_end_screen), kGizmoHandleRadiusPixels,
			is_hot(GizmoHandle::ScaleX) || is_active(GizmoHandle::ScaleX) ? col_hot : col_x
		);

		draw_list->AddCircleFilled(
			ToImGui(y_end_screen), kGizmoHandleRadiusPixels,
			is_hot(GizmoHandle::ScaleY) || is_active(GizmoHandle::ScaleY) ? col_hot : col_y
		);

		auto h{ kGizmoCenterHalfSizePixels };

		auto c0{ GizmoLocalToScreen(
			g.pivot_screen, g.axis_x_screen, g.axis_y_screen, V2_float{ -h.x, -h.y }
		) };
		auto c1{ GizmoLocalToScreen(
			g.pivot_screen, g.axis_x_screen, g.axis_y_screen, V2_float{ h.x, -h.y }
		) };
		auto c2{ GizmoLocalToScreen(
			g.pivot_screen, g.axis_x_screen, g.axis_y_screen, V2_float{ h.x, h.y }
		) };
		auto c3{ GizmoLocalToScreen(
			g.pivot_screen, g.axis_x_screen, g.axis_y_screen, V2_float{ -h.x, h.y }
		) };

		draw_list->AddQuadFilled(
			ToImGui(c0), ToImGui(c1), ToImGui(c2), ToImGui(c3),
			is_hot(GizmoHandle::ScaleUniform) || is_active(GizmoHandle::ScaleUniform) ? col_hot
																					  : col_center
		);
	}
}

void UpdateAndDrawGizmoInstances(
	ImDrawList* draw_list, GizmoState& gizmo, Transform& transform,
	const std::vector<GizmoInstance>& instances, bool viewport_hovered, [[maybe_unused]] bool viewport_focused,
	bool use_local_orientation
) {
	const auto& io{ ImGui::GetIO() };
	V2_float mouse_screen{ io.MousePos.x, io.MousePos.y };

	gizmo.pivot_world = transform.position;

	if (instances.empty()) {
		ResetGizmoInteraction(gizmo);
		return;
	}

	if (gizmo.active == GizmoHandle::None) {
		gizmo.hot = GizmoHandle::None;
		gizmo.hot_occurrence.reset();

		if (viewport_hovered) {
			auto hit{ FindBestGizmoHit(
				instances, transform, gizmo.tool, mouse_screen, use_local_orientation
			) };

			if (hit.has_value()) {
				gizmo.hot			 = hit->handle;
				gizmo.hot_occurrence = hit->occurrence;
			}
		}
	}

	if (viewport_hovered /*&& viewport_focused*/ && gizmo.hot != GizmoHandle::None &&
		gizmo.active == GizmoHandle::None && gizmo.hot_occurrence.has_value() &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		auto* instance{ FindGizmoInstance(instances, gizmo.hot_occurrence.value()) };

		if (instance) {
			auto geometry{ BuildGizmoGeometry(*instance, transform, use_local_orientation) };

			if (geometry.has_value()) {
				BeginGizmoDrag(gizmo, *instance, geometry.value(), transform, mouse_screen);
			}
		}
	}

	if (gizmo.active != GizmoHandle::None && gizmo.active_occurrence.has_value() &&
		ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		auto* instance{ FindGizmoInstance(instances, gizmo.active_occurrence.value()) };

		if (instance) {
			UpdateActiveGizmo(gizmo, *instance, transform, mouse_screen);
		} else {
			ResetGizmoInteraction(gizmo);
		}
	}

	if (gizmo.active != GizmoHandle::None && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		gizmo.active = GizmoHandle::None;
		gizmo.active_occurrence.reset();
	}

	for (const auto& instance : instances) {
		DrawGizmoInstance(draw_list, gizmo, instance, transform, use_local_orientation);
	}
}

} // namespace

void ViewportPanel::DrawSceneCameraOutlines(
	EditorContext& ctx, Viewport presentation_viewport, const FrameContext& frame_context
) {
	if (!use_editor_camera_) {
		return;
	}

	auto scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };

	if (!scene) {
		return;
	}

	auto* draw_list{ ImGui::GetWindowDrawList() };

	float thickness{ 3.0f };

	Frame from{ Frame::World };

	for (auto [c, _camera] : scene->EntitiesWith<impl::CameraData>()) {
		SceneCamera camera{ c };

		auto color{ kCameraOutlineColor };

		if (IsUI(camera)) {
			color = kFixedCameraOutlineColor;
		}

		Rect rect{ camera.GetLogicalViewport().size };
		auto transform{ GetTransform(camera).RelativeTo(GetTransform(camera.GetRenderTarget())) };
		auto world_vertices{ rect.GetWorldVertices(transform) };

		std::array points{
			ToImGui(WorldToScreen(world_vertices[0], frame_context, presentation_viewport, from)),
			ToImGui(WorldToScreen(world_vertices[1], frame_context, presentation_viewport, from)),
			ToImGui(WorldToScreen(world_vertices[2], frame_context, presentation_viewport, from)),
			ToImGui(WorldToScreen(world_vertices[3], frame_context, presentation_viewport, from)),
			ToImGui(WorldToScreen(world_vertices[0], frame_context, presentation_viewport, from))
		};

		// draw_list->AddQuad(points[0], points[1], points[2], points[3], color, thickness);

		draw_list->AddPolyline(
			points.data(), static_cast<int>(points.size()), ToImGui(color), 0, thickness
		);

		// auto center{ ImVec2{ (points[0].x + points[2].x) * 0.5f,
		//					 (points[0].y + points[2].y) * 0.5f } };
	}
}

void ViewportPanel::DrawViewportToolbar(EditorContext& ctx) {
	bool playing{ ctx.editor.IsPlaying() };
	bool paused{ playing && ctx.editor.IsPaused() };

	if (ImGui::Button(playing ? "Stop" : "Play")) {
		if (playing) {
			ctx.editor.Stop();
		} else {
			ctx.editor.Play();
		}
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!playing);

	if (ImGui::Button(paused ? "Resume" : "Pause")) {
		ctx.editor.TogglePause();
	}

	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(!paused);
	ImGui::PushButtonRepeat(true);

	if (ImGui::Button("Step")) {
		ctx.editor.RequestStep();
	}

	ImGui::PopButtonRepeat();
	ImGui::EndDisabled();

	ImGui::SameLine();

	float speed{ ctx.editor.GetTimeScale() };

	ImGui::SetNextItemWidth(120.0f);
	if (ImGui::DragFloat(
			"Speed", &speed, 0.05f, 0.0f, 100.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp
		)) {
		ctx.editor.SetTimeScale(speed);
	}

	ImGui::SameLine();

	if (ImGui::Button(use_editor_camera_ ? "Use Scene Cameras" : "Use Editor Camera")) {
		SetUseEditorCamera(!use_editor_camera_);
	}
}

void SetImageBlendMode(const ImDrawList*, const ImDrawCmd* cmd) {
	auto* renderer_ptr{ static_cast<Renderer*>(cmd->UserCallbackData) };
	PTGN_ASSERT(renderer_ptr);

	impl::RendererAccessor renderer{ *renderer_ptr };

	renderer.SetBlendMode(BlendMode::ReplaceRGBA, true);
}

void ViewportPanel::OnRender(EditorContext& ctx) {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });

	constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoScrollbar |
										ImGuiWindowFlags_NoScrollWithMouse |
										ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;

	ImGui::Begin("Viewport", nullptr, kFlags);
	ImGui::PopStyleVar();

	if (ImGuiWindow* viewport_window = ImGui::FindWindowByName("Viewport")) {
		if (viewport_window->DockNode) {
			viewport_window->DockNode->LocalFlags |= ImGuiDockNodeFlags_HiddenTabBar;
		}
	}

	// Add some horizontal padding for the toolbar only.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 8.0f, 0.0f });

	ImGui::Indent(8.0f);
	DrawViewportToolbar(ctx);
	ImGui::Unindent(8.0f);

	ImGui::PopStyleVar();

	// Remove spacing after the toolbar row.
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);

	// Draw separator without adding spacing after it.
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.0f, 0.0f });

	ImGui::Separator();

	ImGui::PopStyleVar();

	auto min{ FromImGui(ImGui::GetCursorScreenPos()) };
	auto size{ FromImGui(ImGui::GetContentRegionAvail()) };
	auto max{ min + size };

	Viewport presentation_viewport{ .position{ min }, .size{ size } };

	ctx.local.state.viewport.viewport = presentation_viewport;
	ctx.local.state.viewport.focused	= ImGui::IsWindowFocused();
	ctx.local.state.viewport.hovered	= ImGui::IsWindowHovered();

	auto& renderer{ ctx.editor.GetRenderer() };
	auto& window{ ctx.editor.GetWindow() };

	renderer.SetPresentationViewport(presentation_viewport);

	if (size.x <= 0.0f || size.y <= 0.0f) {
		ImGui::End();
		return;
	}

	if (ImGui::IsWindowHovered()) {
		// zoom
		// ctx.editor.camera.Zoom(ImGui::GetIO().MouseWheel);

		// pan
		if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
			// ctx.editor.camera.Pan(ImGui::GetIO().MouseDelta);
		}
	}

	auto draw_list{ ImGui::GetWindowDrawList() };

	auto window_background_color{ window.GetSettings().background_color };

	draw_list->AddRectFilled(ToImGui(min), ToImGui(max), ToImGui(window_background_color));

	auto display_viewport{ renderer.GetDisplayViewport() };
	auto presentation_texture{ ctx.editor.GetPresentationTexture() };
	auto presentation_size{ ctx.editor.GetPresentationTextureSize() };

	if (use_editor_camera_) {
		UpdateEditorCamera(editor_camera_);

		editor_camera_.camera.raw_viewport	 = { .position{}, .size{ 1.0f, 1.0f } };
		editor_camera_.camera.viewport_space = ViewportSpace::Normalized;

		auto logical_viewport{ GetLogicalViewport(
			editor_camera_.camera.raw_viewport, editor_camera_.camera.viewport_space,
			renderer.GetLogicalSize()
		) };

		editor_camera_.camera.view_projection = GetOrthographicViewProjection(
			editor_camera_.camera.transform, logical_viewport.size, editor_camera_.pixel_rounding
		);

		renderer.SetPrimaryWorldCamera(editor_camera_.camera);
	} else {
		renderer.SetPrimaryWorldCamera(std::nullopt);
	}

	auto camera_display_viewport{ GetDisplayViewport(
		editor_camera_.camera.raw_viewport, editor_camera_.camera.viewport_space,
		renderer.GetLogicalSize(), presentation_size, true
	) };

	Viewport viewport{ .position{ min + display_viewport.position },
					   .size{ display_viewport.size } };

	draw_list->AddCallback(SetImageBlendMode, &renderer);

	draw_list->AddImage(
		static_cast<ImTextureID>(presentation_texture), ToImGui(viewport.position),
		ToImGui(viewport.position + viewport.size), ImVec2{ 0.0f, 1.0f }, ImVec2{ 1.0f, 0.0f }
	);

	draw_list->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);

	// Count this draw call so that draw call counts match with and without the editor.
	ctx.editor.GetDebugSystem().stats.Increment("draw_calls");

	if (auto scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };
		scene && use_editor_camera_) {
		FrameContext frame_context{ renderer, GetTransform(scene->GetRenderTarget()),
									presentation_size, editor_camera_.camera.transform,
									camera_display_viewport };

		draw_list->PushClipRect(
			ToImGui(viewport.position), ToImGui(viewport.position + viewport.size), true
		);

		DrawSceneCameraOutlines(ctx, presentation_viewport, frame_context);

		DrawSelectedEntityGizmo(ctx, presentation_viewport, frame_context);

		HandleEntityPicking(ctx, viewport, presentation_size, presentation_viewport, frame_context);

		draw_list->PopClipRect();
	}

	ImGui::End();
}

void ViewportPanel::SetUseEditorCamera(bool use_editor_camera) {
	use_editor_camera_ = use_editor_camera;
}

void ViewportPanel::DrawSelectedEntityGizmo(
	EditorContext& ctx, Viewport presentation_viewport, const FrameContext& frame_context
) {
	auto selected_entity{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };

	if (!selected_entity || !selected_entity.Has<Transform>()) {
		gizmo_entity_uuid_.reset();
		ResetGizmoInteraction(gizmo_state_);
		return;
	}

	auto selected_uuid{ static_cast<std::uint64_t>(selected_entity.Get<UUID>()) };

	if (gizmo_entity_uuid_ != selected_uuid) {
		gizmo_entity_uuid_ = selected_uuid;
		ResetGizmoInteraction(gizmo_state_);
	}

	if (ctx.local.state.viewport.hovered && !ImGui::GetIO().WantTextInput) {
		if (ImGui::IsKeyPressed(ImGuiKey_W)) {
			gizmo_state_.tool = GizmoTool::Translate;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_E)) {
			gizmo_state_.tool = GizmoTool::Rotate;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_R)) {
			gizmo_state_.tool = GizmoTool::Scale;
		}
	}

	auto& scene{ selected_entity.GetScene() };

	Frame direct_frame{ Frame::World };
	Transform editable_transform{ GetWorldTransform(selected_entity) };
	Transform render_target_transform;

	std::vector<EntityRenderPath> render_paths;

	if (selected_entity == scene.GetRenderTarget()) {
		direct_frame = Frame::Display;
		render_paths.push_back(EntityRenderPath{ .id{ kDirectOccurrenceId } });
	} else if (selected_entity.Has<impl::CameraData>()) {
		// Cameras are edited in the coordinate system of their parent render target.
		render_target_transform = GetTransform(SceneCamera{ selected_entity }.GetRenderTarget());
		editable_transform		= editable_transform.RelativeTo(render_target_transform);
		render_paths.push_back(EntityRenderPath{ .id{ kDirectOccurrenceId } });
	} else {
		render_paths = BuildEntityRenderPaths(scene, selected_entity);
	}

	auto gizmo_instances{ BuildGizmoInstances(
		render_paths, editable_transform.position, direct_frame, frame_context,
		presentation_viewport
	) };

	Transform local_transform_before{ GetTransform(selected_entity) };

	auto active_handle_before_update{ gizmo_state_.active };

	UpdateAndDrawGizmoInstances(
		ImGui::GetWindowDrawList(), gizmo_state_, editable_transform, gizmo_instances,
		ctx.local.state.viewport.hovered, ctx.local.state.viewport.focused,
		ctx.editor.GetSettings().gizmo_uses_local_orientation
	);

	auto applied_handle{ gizmo_state_.active != GizmoHandle::None ? gizmo_state_.active
																  : active_handle_before_update };

	if (selected_entity.Has<impl::CameraData>()) {
		editable_transform = editable_transform.InverseRelativeTo(render_target_transform);
	}

	SetWorldTransform(selected_entity, editable_transform);

	Transform local_transform_after{ GetTransform(selected_entity) };

	if (applied_handle != GizmoHandle::None) {
		ApplyGizmoDeltaToButtonVisualTransforms(
			selected_entity, applied_handle, local_transform_before, local_transform_after
		);
	}
}

void ViewportPanel::HandleEntityPicking(
	EditorContext& ctx, Viewport image_viewport, V2_int presentation_framebuffer_size,
	Viewport presentation_viewport, const FrameContext& frame_context
) {
	auto* scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };

	if (!scene) {
		return;
	}

	auto scene_target{ scene->GetRenderTarget() };

	auto scene_framebuffer{
		static_cast<impl::FramebufferId>(scene_target.Get<impl::FramebufferObject>())
	};

	impl::RendererAccessor renderer{ ctx.editor.GetRenderer() };

	if (!renderer.IsEntityPickingEnabled(scene_framebuffer)) {
		return;
	}

	if (!ctx.local.state.viewport.hovered) {
		return;
	}

	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		return;
	}

	if (ImGui::GetIO().WantTextInput) {
		return;
	}

	// Let the gizmo consume the click instead of changing selection.
	if (gizmo_state_.hot != GizmoHandle::None || gizmo_state_.active != GizmoHandle::None) {
		return;
	}

	V2_float mouse_position{
		ImGui::GetIO().MousePos.x,
		ImGui::GetIO().MousePos.y,
	};

	auto scene_target_size{ scene_target.GetSize() };

	PTGN_ASSERT(scene_target_size.IsPositive());
	PTGN_ASSERT(presentation_framebuffer_size.IsPositive());

	auto scene_pixel{ ScreenToFramebufferPixel(mouse_position, image_viewport, scene_target_size) };

	if (!scene_pixel.has_value()) {
		return;
	}

	// Readback must happen after every relevant render target has been completed.
	renderer.FlushBatch();

	auto outer_id{ renderer.ReadEntityId(scene_framebuffer, scene_pixel.value()) };

	auto& hierarchy{ ctx.editor.GetSceneHierarchyPanel() };

	if (!outer_id.has_value() || outer_id.value() == impl::kNoEntityId) {
		hierarchy.SetSelectedEntity({});
		return;
	}

	PTGN_ASSERT(
		outer_id.value() >= 0, "Entity picking returned an invalid entity ID: ", outer_id.value()
	);

	auto outer_entity{ scene->GetEntity(UUID{ outer_id.value() }) };

	if (!outer_entity) {
		hierarchy.SetSelectedEntity({});
		return;
	}

	auto mouse_world{
		ScreenToWorld(mouse_position, frame_context, presentation_viewport, Frame::World)
	};

	auto selected_entity{ ResolveRenderTargetPick(*scene, renderer, outer_entity, mouse_world) };

	hierarchy.SetSelectedEntity(selected_entity);
}

} // namespace ptgn::editor