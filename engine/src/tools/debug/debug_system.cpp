#include "tools/debug/debug_system.h"

#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/frame_context.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/render_queue.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/text/text.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "tools/debug/profiling.h"
#include "tools/debug/stats.h"
#include "tools/debug/debug_settings.h"

namespace ptgn {

namespace {

void DrawPolygonLines(
	Scene& scene, const SceneCamera& camera, std::span<const V2_float> vertices, Color color,
	const FillStyle& fill_style, Depth depth
) {
	if (vertices.size() < 2) {
		return;
	}

	scene.ctx().render_queue.DrawLines(
		vertices, color,
		ShapeRenderParams{
			.fill_style = fill_style,
			.origin		= Origin::Center,
			.depth		= depth,
			.camera		= camera,
			.debug		= true,
		},
		true, {}
	);
}

void DrawDebugColliders(
	Scene& scene, const SceneCamera& camera, const impl::EntityFilterFunc& filter,
	const CollisionDebugSettings& settings
) {
	if (!settings.draw_enabled) {
		return;
	}

	for (auto [entity, collider] : scene.EntitiesWith<Collider>()) {
		// Mask test (entity layers vs camera include/exclude).
		if (filter(entity)) {
			continue;
		}

		auto transform{ GetDrawTransform(entity) };
		auto origin{ entity.GetOrDefault<Origin>() };

		scene.ctx().render_queue.DrawShape(
			transform, collider.shape, settings.draw_color,
			ShapeRenderParams{ .fill_style = settings.draw_fill_style,
							   .origin	   = origin,
							   .camera	   = camera,
							   .debug	   = true }
		);
	}
}

void DrawDebugTextBoundingBoxes(
	Scene& scene, const SceneCamera& camera, const impl::EntityFilterFunc& filter,
	const TextDebugSettings& settings
) {
	if (!settings.draw_enabled) {
		return;
	}

	for (const auto& [entity, data] : std::as_const(scene).EntitiesWith<impl::TextData>()) {
		if (filter(entity)) {
			continue;
		}

		if (!data.text.HasContent()) {
			continue;
		}

		Text text{ entity };
		const auto& layout{ text.GetLayout() };

		auto transform{ GetDrawTransform(entity) };
		auto origin{ entity.GetOrDefault<Origin>() };
		const auto& box{ data.box };

		auto prepared{ impl::PrepareTextDraw(transform, layout, box, origin) };

		if (!prepared.drawable) {
			continue;
		}

		auto shape_params = [&] {
			return ShapeRenderParams{
				.fill_style = settings.draw_line_width,
				.origin		= Origin::Center,
				.camera		= camera,
				.debug		= true,
			};
		};

		auto draw_rect = [&](Rect rect, Color color) {
			if (!rect.GetSize().IsPositive()) {
				return;
			}

			// TODO: Dont translate.
			auto transformed{ prepared.transform };
			transformed.Translate(rect.GetCenter());

			scene.ctx().render_queue.DrawShape(
				transformed, Rect{ rect.GetSize() }, color, shape_params()
			);
		};

		auto draw_line = [&](V2_float start, V2_float end, Color color) {
			scene.ctx().render_queue.DrawLine(
				start, end, color, shape_params(), prepared.transform
			);
		};

		auto layout_bounds{ layout.GetBounds() };

		if (box.HasArea()) {
			// Both dimensions are constrained, so draw the complete text box.
			draw_rect(box.rect, settings.draw_color);
		} else if (box.HasWidth()) {
			// Only width is constrained. Draw the two vertical boundaries
			// where the left and right sides of the text box would be.
			draw_line(
				{ box.rect.min.x, layout_bounds.min.y }, { box.rect.min.x, layout_bounds.max.y },
				settings.draw_color
			);

			draw_line(
				{ box.rect.max.x, layout_bounds.min.y }, { box.rect.max.x, layout_bounds.max.y },
				settings.draw_color
			);
		} else if (box.HasHeight()) {
			// Only height is constrained. Draw the two horizontal boundaries
			// where the top and bottom sides of the text box would be.
			draw_line(
				{ layout_bounds.min.x, box.rect.min.y }, { layout_bounds.max.x, box.rect.min.y },
				settings.draw_color
			);

			draw_line(
				{ layout_bounds.min.x, box.rect.max.y }, { layout_bounds.max.x, box.rect.max.y },
				settings.draw_color
			);
		} else {
			// Unconstrained text has no explicit box, so show its generated
			// logical bounds instead.
			draw_rect(layout_bounds, settings.draw_color);
		}

		// An explicit clip rectangle always has both dimensions and is local
		// to the same prepared text transform.
		if (data.clip.has_value()) {
			draw_rect(data.clip.value().rect, settings.clip_draw_color);
		}
	}
}

void DrawDebugInteractiveShapes(
	Scene& scene, const SceneCamera& camera, const Camera& cam, const RenderTarget& render_target,
	const impl::EntityFilterFunc& filter, const InteractiveDebugSettings& settings
) {
	if (!settings.draw_enabled) {
		return;
	}

	impl::MouseInfo mouse{ scene };

	mouse.position = ConvertPoint(
		mouse.position, Frame::Window, Frame::World,
		FrameContext{ scene.ctx().renderer, render_target, cam }
	);

	scene.ctx().render_queue.DrawPoint(
		mouse.position, settings.draw_color, { .camera = camera, .debug = true }
	);

	for (auto [entity, interactive] : scene.EntitiesWith<impl::Interactive>()) {
		if (!interactive.enabled) {
			continue;
		}

		if (filter(entity)) {
			continue;
		}

		if (auto lock{ entity.TryGet<InteractionLock>() }; lock && lock->block_hover) {
			continue;
		}

		std::vector<std::pair<InteractiveShape, Entity>> shapes;

		impl::GetShapes(entity, entity, shapes);

		for (const auto& [shape, shape_entity] : shapes) {
			auto draw_transform{ GetDrawTransform(shape_entity) };

			scene.ctx().render_queue.DrawShape(
				draw_transform, shape, settings.draw_color,
				{ .fill_style = settings.draw_line_width,
				  .origin	  = shape_entity.GetOrDefault<Origin>(),
				  .camera	  = camera,
				  .debug	  = true }
			);
		}
	}
}

void DrawDebugLightVisibilityPolygons(
	Scene& scene, const SceneCamera& camera, const impl::EntityFilterFunc& filter,
	const LightVisibilityDebugSettings& settings
) {
	if (!settings.draw_enabled) {
		return;
	}

	for (auto [entity, _light, visibility_polygon] :
		 scene.EntitiesWith<LightData, impl::VisibilityPolygon>()) {
		// Mask test: entity layers vs camera include/exclude.
		if (filter(entity)) {
			continue;
		}

		if (visibility_polygon.vertices.empty()) {
			continue;
		}

		auto depth{ GetDepth(entity) };

		DrawPolygonLines(
			scene, camera, visibility_polygon.vertices, settings.polygon_color,
			settings.draw_fill_style, depth
		);

		if (!settings.draw_interiors) {
			continue;
		}

		for (const auto& interior : visibility_polygon.occluder_interiors) {
			auto color{ interior.masks_light_inside ? settings.masks_inside_color
													: settings.does_not_mask_inside_color };

			DrawPolygonLines(
				scene, camera, interior.vertices, color, settings.draw_fill_style, depth
			);
		}
	}
}

} // namespace

namespace impl {

void DrawDebug(
	Scene& scene, const SceneCamera& camera, const Camera& cam, const RenderTarget& render_target,
	const impl::EntityFilterFunc& filter, const DebugSystem& debug
) {
	DrawDebugLightVisibilityPolygons(scene, camera, filter, debug.settings.light);
	DrawDebugTextBoundingBoxes(scene, camera, filter, debug.settings.text);
	DrawDebugColliders(scene, camera, filter, debug.settings.collision);
	DrawDebugInteractiveShapes(scene, camera, cam, render_target, filter, debug.settings.interaction);
}

} // namespace impl

void DebugSystem::SetSettings(const DebugSettings& debug_settings) {
	settings = debug_settings;
}

DebugSettings DebugSystem::GetSettings() const {
	return settings;
}

void DebugSystem::PreUpdate() {
	impl::GetProfiler().timings_.clear();
}

void DebugSystem::PostRender() {
	stats.Reset();
}

} // namespace ptgn
