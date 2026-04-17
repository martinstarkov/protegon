#include "runtime/interaction/interaction_system.h"

#include <algorithm>
#include <chrono>
#include <list>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/input/mouse.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/overlap.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/span.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/frame_context.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/shape.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/draggable_event.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/dropzone_event.h"
#include "runtime/interaction/interactive.h"
#include "runtime/interaction/interactive_event.h"
#include "runtime/interaction/trigger_condition.h"
#include "runtime/physics/bounding_aabb.h"
#include "runtime/physics/broadphase.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scripting/script.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

// TODO: Move these static functions elsewhere.

static void GetShapes(
	Entity entity, Entity root_entity, std::vector<std::pair<InteractiveShape, Entity>>& vector
) {
	bool is_parent{ entity == root_entity };

	const auto get_shape = [&](auto e) {
		if (e.template Has<Rect>()) {
			const auto& rect{ e.template Get<Rect>() };
			vector.emplace_back(rect, e);
		}
		if (e.template Has<Circle>()) {
			const auto& circle{ e.template Get<Circle>() };
			vector.emplace_back(circle, e);
		}
	};

	// Accumulate the shapes of each interactable of the root_entity into the vector.
	if (!is_parent) {
		get_shape(entity);
	}

	// Get sub interactables of the entity recursively.
	if (IsInteractive(entity)) {
		auto interactables{ GetInteractiveShapes(entity) };
		for (const auto& interactable : interactables) {
			GetShapes(interactable, root_entity, vector);
		}
	}

	// Once recursion is completed, there should be at least one interactable shape on an
	// interactive entity.
	if (is_parent) {
		if (vector.empty()) {
			get_shape(root_entity);
		}
		PTGN_ASSERT(
			!vector.empty(), "Failed to find a valid interactable for the entity: ", entity
		);
	}
}

namespace impl {

MouseInfo::MouseInfo(const Scene& scene) {
	const SceneInput& input{ scene.ctx().input };
	position	  = input.GetMousePosition(Frame::Window);
	scroll_delta  = input.GetMouseScroll();
	left_held	  = input.MouseHeld(Mouse::Left);
	left_pressed  = input.MousePressed(Mouse::Left);
	left_released = input.MouseReleased(Mouse::Left);
}

} // namespace impl

Transform InteractionSystem::GetWorldOffsetTransform(const Shape& shape, Entity shape_entity) {
	auto transform{ GetWorldTransform(shape_entity) };

	transform = OffsetByOrigin(shape, transform, shape_entity);

	return transform;
}

bool InteractionSystem::Overlap(V2_float point, Entity interactive_entity) {
	std::vector<std::pair<InteractiveShape, Entity>> shapes;
	GetShapes(interactive_entity, interactive_entity, shapes);

	PTGN_ASSERT(!shapes.empty(), "Cannot check for overlap with an interactive that has no shape");

	for (const auto& [shape, e] : shapes) {
		auto transform{ GetWorldOffsetTransform(shape, e) };
		if (ptgn::Overlap(point, transform, shape)) {
			return true;
		}
	}

	return false;
}

bool InteractionSystem::Overlap(Entity entityA, Entity entityB) {
	std::vector<std::pair<InteractiveShape, Entity>> shapesA;
	GetShapes(entityA, entityA, shapesA);

	std::vector<std::pair<InteractiveShape, Entity>> shapesB;
	GetShapes(entityB, entityB, shapesB);

	PTGN_ASSERT(
		!shapesA.empty() && !shapesB.empty(),
		"Cannot check for overlap with an interactive that has no shape"
	);

	for (const auto& [shapeA, eA] : shapesA) {
		auto transformA{ GetWorldOffsetTransform(shapeA, eA) };
		for (const auto& [shapeB, eB] : shapesB) {
			auto transformB{ GetWorldOffsetTransform(shapeB, eB) };
			if (ptgn::Overlap(transformA, shapeA, transformB, shapeB)) {
				return true;
			}
		}
	}

	return false;
}

bool InteractionSystem::IsAnyDragging(Camera camera) const {
	auto it{ dragging_entities_.find(camera) };

	if (it == dragging_entities_.end()) {
		return false;
	}

	return !it->second.empty();
}

bool InteractionSystem::IsTopOnly() const {
	return top_only_;
}

void InteractionSystem::SetTopOnly(bool top_only) {
	top_only_ = top_only;
}

void InteractionSystem::SetDebugSettings(const InteractiveDebugSettings& settings) {
	debug_settings_ = settings;
}

void InteractionSystem::DrawDebug(Scene& scene) const {
	if (!debug_settings_.draw_enabled) {
		return;
	}

	const impl::MouseInfo mouse_state{ scene };

	std::vector<Entity> cameras;

	for (auto [camera, _cam] : scene.EntitiesWith<impl::CameraData>()) {
		cameras.emplace_back(camera);
	}

	SortByDepth(cameras, false);

	for (const Entity& camera_entity : cameras) {
		Camera camera{ camera_entity };

		RenderTarget render_target;

		if (auto rt{ camera.TryGet<impl::ParentRenderTarget>() }) {
			render_target = rt->render_target;
		} else {
			render_target = scene.GetRenderTarget();
		}

		PTGN_ASSERT(render_target);

		impl::MouseInfo mouse{ mouse_state };

		mouse.position = ConvertPoint(
			mouse.position, Frame::Window, Frame::Camera,
			FrameContext{ scene.ctx().renderer, render_target, camera }
		);

		if (debug_settings_.draw_enabled) {
			scene.ctx().debug.DrawPoint(mouse.position, debug_settings_.draw_color, camera);
		}

		for (auto [entity, interactive] : scene.EntitiesWith<impl::Interactive>()) {
			if (!interactive.enabled) {
				continue;
			}
			if (!camera.IsVisible(entity)) {
				continue;
			}

			if (auto lock{ entity.TryGet<InteractionLock>() }; lock && lock->block_hover) {
				continue;
			}

			std::vector<std::pair<InteractiveShape, Entity>> shapes;

			GetShapes(entity, entity, shapes);

			for (const auto& [shape, shape_entity] : shapes) {
				auto draw_transform{ GetDrawTransform(shape_entity) };

				scene.ctx().debug.DrawShape(
					shape, draw_transform, debug_settings_.draw_color,
					debug_settings_.draw_line_width, GetDrawOrigin(shape_entity), camera
				);
			}
		}
	}
}

InteractionSystem::InteractiveEntities InteractionSystem::GetInteractiveEntities(
	const impl::MouseInfo& mouse_state, const std::vector<Entity>& all_entities
) const {
	impl::KDTree tree{ 20 };
	std::vector<impl::KDObject> objects;

	std::unordered_map<Entity, std::vector<std::pair<InteractiveShape, Entity>>> entity_shapes;

	for (Entity entity : all_entities) {
		if (auto lock{ entity.TryGet<InteractionLock>() }; lock && lock->block_hover) {
			PTGN_ASSERT(
				lock->remaining_time >= secondsf{ 0.0f }, "Interaction time cannot be negative"
			);
			continue;
		}

		std::vector<std::pair<InteractiveShape, Entity>> shapes;

		GetShapes(entity, entity, shapes);

		entity_shapes.try_emplace(entity, shapes);

		for (const auto& [shape, shape_entity] : shapes) {
			auto transform{ GetWorldOffsetTransform(shape, shape_entity) };
			objects.emplace_back(entity, GetBoundingAABB(shape, transform));
		}
	}

	tree.Build(objects);

	// Broadphase check.
	auto candidates{ tree.Query(mouse_state.position) };

	VectorRemoveDuplicates(candidates);

	InteractiveEntities entities;
	entities.under_mouse.reserve(candidates.size());

	for (const auto& entity : candidates) {
		PTGN_ASSERT(
			entity_shapes.contains(entity),
			"Entity cannot be candidate in broadphase without a shape"
		);

		const auto& shapes{ entity_shapes.find(entity)->second };

		for (const auto& [shape, shape_entity] : shapes) {
			if (std::ranges::contains(entities.under_mouse, entity)) {
				continue;
			}

			auto transform{ GetWorldOffsetTransform(shape, shape_entity) };

			if (ptgn::Overlap(mouse_state.position, transform, shape)) {
				PTGN_ASSERT(
					!std::ranges::contains(entities.under_mouse, entity),
					"Attempting to check same interactive entity under mouse twice"
				);
				entities.under_mouse.emplace_back(entity);
			}
		}
	}

	if (top_only_ && !entities.under_mouse.empty()) {
		// Find the draggable with the highest depth.
		auto draggable_it{
			std::ranges::max_element(entities.under_mouse, impl::EntityDepthCompare{ true })
		};

		// If no draggable is found, find the interactive entity with the highest depth.
		if (!draggable_it->Has<impl::Draggable>() ||
			!draggable_it->Get<impl::Draggable>().enabled) {
			draggable_it =
				std::ranges::max_element(entities.under_mouse, impl::EntityDepthCompare{ true });
		}

		PTGN_ASSERT(draggable_it != entities.under_mouse.end());

		entities.under_mouse = { *draggable_it };
	}

	auto all{ all_entities };

	std::erase_if(all, [&entities](const auto& x) {
		return std::ranges::contains(entities.under_mouse, x);
	});

	entities.not_under_mouse = all;

	return entities;
}

std::vector<Entity> InteractionSystem::GetDropzones(const Scene& scene) {
	std::vector<Entity> objects;

	for (auto [entity, interactive, dropzone] :
		 scene.EntitiesWith<impl::Interactive, impl::Dropzone>()) {
		if (!interactive.enabled || !dropzone.enabled) {
			continue;
		}

		objects.emplace_back(entity);
	}

	return objects;
}

void InteractionSystem::UpdateMouseOverStates(
	const std::vector<Entity>& current, const std::vector<Entity>& last_mouse_over
) {
	for (const Entity& e : current) {
		if (!e.Has<impl::Scripts>()) {
			continue;
		}
		if (!std::ranges::contains(last_mouse_over, e)) {
			PushEvent<event::MouseEnter>(e);
		}
	}

	for (const Entity& e : last_mouse_over) {
		if (!e.Has<impl::Scripts>()) {
			continue;
		}
		if (!std::ranges::contains(current, e)) {
			PushEvent<event::MouseLeave>(e);
		}
	}
}

bool InteractionSystem::IsOverlappingDropzone(
	const V2_float& mouse_position, const Entity& draggable, const Entity& dropzone,
	TriggerCondition condition
) {
	bool is_overlapping{ false };
	switch (condition) {
		using enum TriggerCondition;
		case MouseOverlaps: {
			is_overlapping = Overlap(mouse_position, dropzone);
			break;
		}
		case TransformOverlaps: {
			PTGN_ASSERT(
				GetMask(draggable) == GetMask(dropzone),
				"Dropzone entity and drag entity must share the same camera"
			);
			// Origin not accounted for because this is about TransformOverlaps, not center.
			auto position{ GetWorldTransform(draggable).GetPosition() };
			is_overlapping = Overlap(position, dropzone);
			break;
		}
		case Overlaps: {
			PTGN_ASSERT(
				GetMask(draggable) == GetMask(dropzone),
				"Dropzone entity and drag entity must share the same camera"
			);
			is_overlapping = Overlap(draggable, dropzone);
			break;
		}
		case Contains:
			// TODO: Implement.
			PTGN_ERROR("Unimplemented drop condition");
			break;
		case None: break;
		default:   PTGN_ERROR("Unrecognized drop condition");
	}
	return is_overlapping;
}

void InteractionSystem::HandleDragging(
	const std::vector<Entity>& over, const std::vector<Entity>& dropzones,
	const impl::MouseInfo& mouse, std::vector<Entity>& dragging_entities
) {
	// Start dragging
	if (mouse.left_pressed) {
		for (Entity dragging : over) {
			if (!dragging.Has<impl::Draggable>() || !dragging.Get<impl::Draggable>().enabled) {
				continue;
			}

			if (std::ranges::contains(dragging_entities, dragging)) {
				continue; // Already dragging this
			}

			dragging_entities.emplace_back(dragging);

			PushEvent<event::DragStart>(dragging, mouse.position);

			if (!dragging.Has<impl::Draggable>() || !dragging.Get<impl::Draggable>().enabled) {
				continue;
			}

			for (Entity dropzone : dropzones) {
				PTGN_ASSERT((dropzone.Has<impl::Dropzone, impl::Interactive>()));
				PTGN_ASSERT(dropzone.Get<impl::Interactive>().enabled);
				PTGN_ASSERT(dropzone.Get<impl::Dropzone>().enabled);
				if (dropzone == dragging) {
					continue;
				}

				// Only allow pickup from dropzones this draggable was actually dropped into.
				if (!std::ranges::contains(dropzone.Get<impl::Dropzone>().draggables, dragging)) {
					continue;
				}

				AddDropzoneActions<DropzoneAction::Pickup>(
					dragging, dropzone, mouse.position,
					[&]() {
						std::erase(dropzone.Get<impl::Dropzone>().draggables, dragging);
						PushEvent<event::PickupFromDropzone>(dropzone, dragging);
					},
					[&]() {
						std::erase(dragging.Get<impl::Draggable>().dropzones, dropzone);
						PushEvent<event::PickupDraggable>(dragging, dropzone);
					},
					[]() {}
				);
			}

			if (!dragging.Has<impl::Draggable>() || !dragging.Get<impl::Draggable>().enabled) {
				continue;
			}

			auto& draggable{ dragging.Get<impl::Draggable>() };

			draggable.dragging = true;
			draggable.start	   = mouse.position;
			// Origin does not need to be accounted for here because offset will be used to set
			// the position (most often).
			draggable.offset = GetWorldTransform(dragging).GetPosition() - draggable.start;
		}
	}

	// Continue dragging
	if (mouse.left_held || mouse.left_pressed) {
		for (Entity dragging : dragging_entities) {
			if (!dragging.Has<impl::Draggable>() || !dragging.Get<impl::Draggable>().enabled) {
				continue;
			}
			auto offset{ dragging.Get<impl::Draggable>().offset };
			auto position{ mouse.position + offset };
			PushEvent<event::Drag>(dragging, position, offset);
		}
	}

	// Stop dragging
	if (mouse.left_released) {
		for (Entity dragging : dragging_entities) {
			if (!dragging.Has<impl::Draggable>() || !dragging.Get<impl::Draggable>().enabled ||
				!dragging.Has<impl::Interactive>() || !dragging.Get<impl::Interactive>().enabled) {
				continue;
			}

			PushEvent<event::DragStop>(dragging, mouse.position);

			if (!dragging.Has<impl::Draggable>() || !dragging.Get<impl::Draggable>().enabled ||
				!dragging.Has<impl::Interactive>() || !dragging.Get<impl::Interactive>().enabled) {
				continue;
			}

			for (Entity dropzone : dropzones) {
				PTGN_ASSERT((dropzone.Has<impl::Dropzone, impl::Interactive>()));
				PTGN_ASSERT(dropzone.Get<impl::Interactive>().enabled);
				PTGN_ASSERT(dropzone.Get<impl::Dropzone>().enabled);
				if (dropzone == dragging) {
					continue;
				}

				AddDropzoneActions<DropzoneAction::Drop>(
					dragging, dropzone, mouse.position,
					[&]() {
						if (!std::ranges::contains(
								dropzone.Get<impl::Dropzone>().draggables, dragging
							)) {
							dropzone.Get<impl::Dropzone>().draggables.emplace_back(dragging);
							PushEvent<event::DropIntoDropzone>(dropzone, dragging);
						}
					},
					[&]() {
						if (!std::ranges::contains(
								dragging.Get<impl::Draggable>().dropzones, dropzone
							)) {
							dragging.Get<impl::Draggable>().dropzones.emplace_back(dropzone);
							PushEvent<event::DropDraggable>(dragging, dropzone);
						}
					},
					[]() {}
				);
			}

			if (!dragging.Has<impl::Draggable>() || !dragging.Get<impl::Draggable>().enabled ||
				!dragging.Has<impl::Interactive>() || !dragging.Get<impl::Interactive>().enabled) {
				continue;
			}

			auto& draggable{ dragging.Get<impl::Draggable>() };
			draggable.dragging = false;
			draggable.start	   = {};
			draggable.offset   = {};
		}
		dragging_entities.clear(); // End all drags
	}
}

void InteractionSystem::CleanupDropzones(const std::vector<Entity>& dropzones) {
	for (Entity dropzone : dropzones) {
		if (!dropzone.Has<impl::Dropzone>() || !dropzone.Get<impl::Dropzone>().enabled) {
			continue;
		}

		auto& dropped{ dropzone.Get<impl::Dropzone>().draggables };

		std::erase_if(dropped, [](const Entity& e) {
			return !e || !e.Has<impl::Draggable>() || !e.Get<impl::Draggable>().enabled ||
				   !e.Has<impl::Interactive>() || !e.Get<impl::Interactive>().enabled;
		});
	}
}

void InteractionSystem::HandleDropzones(
	const std::vector<Entity>& dropzones, const impl::MouseInfo& mouse,
	const std::vector<Entity>& dragging_entities
) {
	// 1. Compute which dropzones each dragged entity is currently over
	for (Entity dragging : dragging_entities) {
		if (!dragging.Has<impl::Draggable>() || !dragging.Get<impl::Draggable>().enabled) {
			continue;
		}

		auto& draggable{ dragging.Get<impl::Draggable>() };
		draggable.hovered_dropzones = {};

		for (Entity dropzone : dropzones) {
			PTGN_ASSERT((dropzone.Has<impl::Dropzone, impl::Interactive>()));
			PTGN_ASSERT(dropzone.Get<impl::Interactive>().enabled);
			PTGN_ASSERT(dropzone.Get<impl::Dropzone>().enabled);
			if (dragging == dropzone) {
				continue;
			}

			bool entered{ !std::ranges::contains(draggable.last_hovered_dropzones, dropzone) };

			AddDropzoneActions<DropzoneAction::Move>(
				dragging, dropzone, mouse.position,
				[&]() {
					if (entered) {
						PushEvent<event::EnterDropzone>(dropzone, dragging);
						PushEvent<event::MoveOverDropzone>(dropzone, dragging);
					} else {
						PushEvent<event::MoveOverDropzone>(dropzone, dragging);
					}
				},
				[&]() {
					if (entered) {
						PushEvent<event::DragEnter>(dragging, dropzone);
						PushEvent<event::DragOver>(dragging, dropzone);
					} else {
						PushEvent<event::DragOver>(dragging, dropzone);
					}
				},
				[&]() {
					if (!std::ranges::contains(draggable.hovered_dropzones, dropzone)) {
						draggable.hovered_dropzones.emplace_back(dropzone);
					}
				}
			);
		}

		if (!dragging.Has<impl::Draggable>() || !dragging.Get<impl::Draggable>().enabled) {
			continue;
		}

		// 2. Handle leaving dropzones
		for (Entity last_dropzone : draggable.last_hovered_dropzones) {
			if (dragging == last_dropzone) {
				continue;
			}
			if (std::ranges::contains(draggable.hovered_dropzones, last_dropzone)) {
				continue;
			}
			if (last_dropzone.Has<impl::Dropzone, impl::Interactive>() &&
				last_dropzone.Get<impl::Interactive>().enabled &&
				last_dropzone.Get<impl::Dropzone>().enabled) {
				PushEvent<event::LeaveDropzone>(last_dropzone, dragging);
			}
			PushEvent<event::DragLeave>(dragging, last_dropzone);
		}

		if (!dragging.Has<impl::Draggable>() || !dragging.Get<impl::Draggable>().enabled) {
			continue;
		}

		// 3. Always call DragOut if not currently over a dropzone
		for (Entity dropzone : dropzones) {
			PTGN_ASSERT((dropzone.Has<impl::Dropzone, impl::Interactive>()));
			PTGN_ASSERT(dropzone.Get<impl::Interactive>().enabled);
			PTGN_ASSERT(dropzone.Get<impl::Dropzone>().enabled);
			if (dragging == dropzone) {
				continue;
			}
			if (std::ranges::contains(draggable.hovered_dropzones, dropzone)) {
				continue;
			}
			PushEvent<event::MoveOutsideDropzone>(dropzone, dragging);
			PushEvent<event::DragOut>(dragging, dropzone);
		}

		if (!dragging.Has<impl::Draggable>() || !dragging.Get<impl::Draggable>().enabled) {
			continue;
		}

		// Store current for next frame.
		draggable.last_hovered_dropzones = draggable.hovered_dropzones;
	}
}

void InteractionSystem::DispatchMouseEvents(
	const std::vector<Entity>& over, const std::vector<Entity>& out, const impl::MouseInfo& mouse
) {
	for (Entity e : over) {
		if (!e.Has<impl::Scripts>()) {
			continue;
		}

		if (auto lock{ e.TryGet<InteractionLock>() }; lock && lock->block_press) {
			PTGN_ASSERT(
				lock->remaining_time >= secondsf{ 0.0f }, "Interaction time cannot be negative"
			);
			continue;
		}

		PushEvent<event::MouseMoveOver>(e);

		if (mouse.left_pressed) {
			PushEvent<event::MousePressedOver>(e, Mouse::Left);
		}
		if (mouse.left_held) {
			PushEvent<event::MouseHeldOver>(e, Mouse::Left);
		}
		if (mouse.left_released) {
			PushEvent<event::MouseReleasedOver>(e, Mouse::Left);
		}
		if (!mouse.scroll_delta.IsZero()) {
			PushEvent<event::MouseScrollOver>(e, mouse.scroll_delta);
		}
	}

	for (const Entity& e : out) {
		if (!e.Has<impl::Scripts>()) {
			continue;
		}

		if (std::ranges::contains(over, e)) {
			continue;
		}

		PushEvent<event::MouseMoveOut>(e);

		if (mouse.left_pressed) {
			PushEvent<event::MousePressedOut>(e, Mouse::Left);
		}
		if (mouse.left_held) {
			PushEvent<event::MouseHeldOut>(e, Mouse::Left);
		}
		if (mouse.left_released) {
			PushEvent<event::MouseReleasedOut>(e, Mouse::Left);
		}
		if (!mouse.scroll_delta.IsZero()) {
			PushEvent<event::MouseScrollOut>(e, mouse.scroll_delta);
		}
	}
}

void InteractionSystem::Update(Scene& scene) {
	secondsf dt{ scene.ctx().dt() };

	for (auto [entity, lock] : scene.EntitiesWith<InteractionLock>()) {
		lock.remaining_time -= dt;

		if (lock.remaining_time <= secondsf{ 0.0f }) {
			entity.Remove<InteractionLock>();
		}
	}

	const impl::MouseInfo mouse_state{ scene };

	std::vector<Entity> cameras;

	for (auto [camera, _cam] : scene.EntitiesWith<impl::CameraData>()) {
		cameras.emplace_back(camera);
	}

	SortByDepth(cameras, false);

	bool handled_under_mouse{ false };

	for (const Entity& camera_entity : cameras) {
		Camera camera{ camera_entity };

		RenderTarget render_target;

		if (auto rt{ camera.TryGet<impl::ParentRenderTarget>() }) {
			render_target = rt->render_target;
		} else {
			render_target = scene.GetRenderTarget();
		}

		PTGN_ASSERT(render_target);

		impl::MouseInfo mouse{ mouse_state };

		mouse.position = ConvertPoint(
			mouse.position, Frame::Window, Frame::Camera,
			FrameContext{ scene.ctx().renderer, render_target, camera }
		);

		std::vector<Entity> camera_entities;

		for (auto [entity, interactive] : scene.EntitiesWith<impl::Interactive>()) {
			if (!interactive.enabled) {
				continue;
			}
			if (!camera.IsVisible(entity)) {
				continue;
			}
			camera_entities.emplace_back(entity);
		}

		auto entities = GetInteractiveEntities(mouse, camera_entities);

		if (top_only_ && handled_under_mouse) {
			entities.under_mouse	 = {};
			entities.not_under_mouse = camera_entities;
		}

		if (!entities.under_mouse.empty()) {
			handled_under_mouse = true;
		}

		auto dropzones{ GetDropzones(scene) };

		auto& dragging_entities = dragging_entities_[camera];
		auto& last_mouse_over	= last_mouse_over_[camera];

		UpdateMouseOverStates(entities.under_mouse, last_mouse_over);

		DispatchMouseEvents(entities.under_mouse, entities.not_under_mouse, mouse);

		HandleDragging(entities.under_mouse, dropzones, mouse, dragging_entities);

		if (IsAnyDragging(camera)) {
			HandleDropzones(dropzones, mouse, dragging_entities);
		}

		CleanupDropzones(dropzones);

		std::erase_if(dragging_entities, [](const auto& entity) {
			return !entity.template Has<impl::Draggable>() ||
				   !entity.template Get<impl::Draggable>().enabled;
		});

		// Save for next frame.
		last_mouse_over = entities.under_mouse;
	}

	// Remove deleted cameras.

	std::erase_if(dragging_entities_, [&cameras](const auto& pair) {
		return !std::ranges::contains(cameras, Entity{ pair.first });
	});

	std::erase_if(last_mouse_over_, [&cameras](const auto& pair) {
		return !std::ranges::contains(cameras, Entity{ pair.first });
	});
}

} // namespace ptgn