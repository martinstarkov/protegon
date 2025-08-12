#include "scene/scene_input.h"

#include <unordered_set>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/interactive.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/script.h"
#include "debug/log.h"
#include "ecs/ecs.h"
#include "input/input_handler.h"
#include "input/mouse.h"
#include "math/geometry.h"
#include "math/geometry/circle.h"
#include "math/geometry/rect.h"
#include "math/overlap.h"
#include "math/vector2.h"
#include "physics/collision/broadphase.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_key.h"
#include "scene/scene_manager.h"
#include "utility/span.h"

// TODO: Ensure that entities are alive and interactable, etc, after each script hook is
// called in case the hook deletes or removes components from itself.
// Also check for this issue for other script hooks, e.g. buttons.

// TODO: Fix dropzone triggers.

// TODO: Fix top only mode.

namespace ptgn {

MouseInfo::MouseInfo() :
	position{ game.input.GetMousePosition() },
	scroll_delta{ game.input.GetMouseScroll() },
	left_pressed{ game.input.MousePressed(Mouse::Left) },
	left_down{ game.input.MouseDown(Mouse::Left) },
	left_up{ game.input.MouseUp(Mouse::Left) } {}

void GetShapes(
	const Entity& entity, const Entity& root_entity, std::vector<std::pair<Shape, Entity>>& vector
) {
	bool is_parent{ entity == root_entity };

	// Accumulate the shapes of each interactable of the root_entity into the vector.
	if (!is_parent) {
		if (entity.Has<Rect>()) {
			const auto& rect{ entity.Get<Rect>() };
			// TODO: Instead of using the root entity, pass the origin offset along the recursive
			// chain to accumulate it.
			auto offset_rect{ ApplyOffset(rect, root_entity) };
			vector.emplace_back(offset_rect, entity);
		}
		if (entity.Has<Circle>()) {
			const auto& circle{ entity.Get<Circle>() };
			vector.emplace_back(circle, entity);
		}
	}

	// Get sub interactables of the entity recursively.
	if (IsInteractive(entity)) {
		auto interactables{ GetInteractables(entity) };
		for (const auto& interactable : interactables) {
			GetShapes(interactable, root_entity, vector);
		}
	}

	// Once recursion is completed, there should be at least one interactable shape on an
	// interactive entity.
	if (is_parent) {
		PTGN_ASSERT(
			!vector.empty(), "Failed to find a valid interactable for the entity: ", entity.GetId()
		);
	}
}

bool Overlap(const V2_float& point, const Entity& entity) {
	std::vector<std::pair<Shape, Entity>> shapes;
	GetShapes(entity, entity, shapes);

	PTGN_ASSERT(!shapes.empty(), "Cannot check for overlap with an interactive that has no shape");

	for (const auto& [shape, e] : shapes) {
		auto transform{ GetAbsoluteTransform(e) };
		if (Overlap(point, transform, shape)) {
			return true;
		}
	}

	return false;
}

bool Overlap(const Entity& entityA, const Entity& entityB) {
	std::vector<std::pair<Shape, Entity>> shapesA;
	GetShapes(entityA, entityA, shapesA);

	std::vector<std::pair<Shape, Entity>> shapesB;
	GetShapes(entityB, entityB, shapesB);

	PTGN_ASSERT(
		!shapesA.empty() && !shapesB.empty(),
		"Cannot check for overlap with an interactive that has no shape"
	);

	for (const auto& [shapeA, eA] : shapesA) {
		auto transformA{ GetAbsoluteTransform(eA) };
		for (const auto& [shapeB, eB] : shapesB) {
			auto transformB{ GetAbsoluteTransform(eB) };
			if (Overlap(transformA, shapeA, transformB, shapeB)) {
				return true;
			}
		}
	}

	return false;
}

std::vector<Entity> SceneInput::GetEntitiesUnderMouse(Scene& scene, const MouseInfo& mouse_state) {
	impl::KDTree tree{ 20 };
	std::vector<impl::KDObject> objects;
	for (auto [entity, interactive] : scene.InternalEntitiesWith<Interactive>()) {
		auto transform{ GetAbsoluteTransform(entity) };
		std::vector<std::pair<Shape, Entity>> shapes;
		GetShapes(entity, entity, shapes);
		for (auto [shape, _] : shapes) {
			objects.emplace_back(entity, GetBoundingAABB(shape, transform));
		}
	}
	tree.Build(objects);
	// 2. Get entities under mouse using KD-tree
	auto under_mouse = tree.Query(mouse_state.position);
	return under_mouse;
}

std::vector<Entity> SceneInput::GetDropzones(Scene& scene) {
	std::vector<Entity> objects{ scene.InternalEntitiesWith<Interactive, Dropzone>().GetVector() };
	return objects;
}

// Called every frame
void SceneInput::UpdateMouseOverStates(const std::vector<Entity>& current) {
	for (Entity e : current) {
		if (!e.Has<Scripts>()) {
			continue;
		}
		if (!last_mouse_over_.contains(e)) {
			e.Get<Scripts>().AddAction(&MouseScript::OnMouseEnter);
		}
	}

	for (Entity e : last_mouse_over_) {
		if (!e.Has<Scripts>()) {
			continue;
		}
		if (!VectorContains(current, e)) {
			e.Get<Scripts>().AddAction(&MouseScript::OnMouseLeave);
		}
	}
}

void SceneInput::DispatchMouseEvents(const std::vector<Entity>& over, const MouseInfo& mouse) {
	for (Entity e : over) {
		if (!e.Has<Scripts>()) {
			continue;
		}
		auto& scripts{ e.Get<Scripts>() };
		scripts.AddAction(&MouseScript::OnMouseMoveOver);
		if (mouse.left_down) {
			scripts.AddAction(&MouseScript::OnMouseDownOver, Mouse::Left);
		}
		if (mouse.left_pressed || mouse.left_down) {
			scripts.AddAction(&MouseScript::OnMousePressedOver, Mouse::Left);
		}
		if (mouse.left_up) {
			scripts.AddAction(&MouseScript::OnMouseUpOver, Mouse::Left);
		}
		if (!mouse.scroll_delta.IsZero()) {
			scripts.AddAction(&MouseScript::OnMouseScrollOver, mouse.scroll_delta);
		}
	}

	for (Entity e : last_mouse_over_) {
		if (!e.Has<Scripts>()) {
			continue;
		}
		if (VectorContains(over, e)) {
			continue;
		}
		auto& scripts{ e.Get<Scripts>() };
		scripts.AddAction(&MouseScript::OnMouseMoveOut);
		if (mouse.left_down) {
			scripts.AddAction(&MouseScript::OnMouseDownOut, Mouse::Left);
		}
		if (mouse.left_pressed || mouse.left_down) {
			scripts.AddAction(&MouseScript::OnMousePressedOut, Mouse::Left);
		}
		if (mouse.left_up) {
			scripts.AddAction(&MouseScript::OnMouseUpOut, Mouse::Left);
		}
		if (!mouse.scroll_delta.IsZero()) {
			scripts.AddAction(&MouseScript::OnMouseScrollOut, mouse.scroll_delta);
		}
	}
}

void SceneInput::HandleDragging(
	const std::vector<Entity>& over, const std::vector<Entity>& dropzones, const MouseInfo& mouse
) {
	// Start dragging
	if (mouse.left_down) {
		for (Entity dragging : over) {
			if (dragging_entities.contains(dragging)) {
				continue; // Already dragging this
			}

			dragging_entities[dragging].drag_start_position = mouse.position;

			auto scripts{ dragging.TryGet<Scripts>() };

			if (scripts) {
				scripts->AddAction(&DragScript::OnDragStart, mouse.position);
			}

			// Pickup event for any dropzones that the dragging object overlaps.
			for (Entity dropzone : dropzones) {
				if (dropzone == dragging) {
					continue;
				}
				if (!dropzone.Has<Dropzone, Interactive>()) {
					continue;
				}
				if (!Overlap(dragging, dropzone)) {
					continue;
				}
				if (auto dropzone_scripts{ dropzone.TryGet<Scripts>() }) {
					dropzone_scripts->AddAction(&DropzoneScript::OnDraggablePickup, dragging);
				}
				if (scripts) {
					scripts->AddAction(&DragScript::OnPickup, dropzone);
				}
			}

			if (!dragging.Has<Draggable>()) {
				continue;
			}

			auto& draggable{ dragging.Get<Draggable>() };

			draggable.dragging = true;
			draggable.start	   = mouse.position;
			draggable.offset   = GetAbsolutePosition(dragging) - draggable.start;
		}
	}

	// Continue dragging
	if (mouse.left_pressed || mouse.left_down) {
		for (auto& pair : dragging_entities) {
			auto dragging{ pair.first };
			auto scripts{ dragging.TryGet<Scripts>() };

			if (scripts) {
				scripts->AddAction(&DragScript::OnDrag);
			}
		}
	}

	// Stop dragging
	if (mouse.left_up) {
		for (auto& pair : dragging_entities) {
			auto dragging{ pair.first };
			auto scripts{ dragging.TryGet<Scripts>() };

			if (scripts) {
				scripts->AddAction(&DragScript::OnDragStop, mouse.position);
			}

			for (Entity dropzone : dropzones) {
				if (dropzone == dragging) {
					continue;
				}
				if (!dropzone.Has<Dropzone, Interactive>()) {
					continue;
				}
				// TODO: Add DropzoneTrigger nuance here.
				if (!Overlap(dragging, dropzone)) {
					continue;
				}
				if (auto dropzone_scripts{ dropzone.TryGet<Scripts>() }) {
					dropzone_scripts->AddAction(&DropzoneScript::OnDraggableDrop, dragging);
				}
				if (scripts) {
					scripts->AddAction(&DragScript::OnDrop, dropzone);
				}
			}

			if (!dragging.Has<Draggable>()) {
				continue;
			}

			auto& draggable{ dragging.Get<Draggable>() };
			draggable.dragging = false;
			draggable.start	   = {};
			draggable.offset   = {};
		}
		dragging_entities.clear(); // End all drags
	}
}

void SceneInput::HandleDropzones(
	const std::vector<Entity>& mouse_over, const std::vector<Entity>& dropzones,
	const MouseInfo& mouse
) {
	// TODO: Fix dropzones.

	std::unordered_map<Entity, std::unordered_set<Entity>> current_dragged_dropzones;

	// 1. Compute which dropzones each dragged entity is currently over
	for (auto& pair : dragging_entities) {
		auto dragging{ pair.first };
		std::unordered_set<Entity> over_now;
		auto scripts{ dragging.TryGet<Scripts>() };

		for (Entity dropzone : dropzones) {
			if (!dropzone.Has<Dropzone, Interactive>()) {
				continue;
			}
			if (dragging == dropzone) {
				continue;
			}
			// TODO: Add DropzoneTrigger nuance here.
			if (!Overlap(dragging, dropzone)) {
				continue;
			}
			over_now.insert(dropzone);

			// Wasn't over this last frame -> Enter
			if (!last_dragged_dropzones[dragging].contains(dropzone)) {
				if (auto dropzone_scripts{ dropzone.TryGet<Scripts>() }) {
					dropzone_scripts->AddAction(&DropzoneScript::OnDraggableEnter, dragging);
					dropzone_scripts->AddAction(&DropzoneScript::OnDraggableOver, dragging);
				}
				if (scripts) {
					scripts->AddAction(&DragScript::OnDragEnter, dropzone);
					scripts->AddAction(&DragScript::OnDragOver, dropzone);
				}
			} else {
				if (auto dropzone_scripts{ dropzone.TryGet<Scripts>() }) {
					dropzone_scripts->AddAction(&DropzoneScript::OnDraggableOver, dragging);
				}
				if (scripts) {
					scripts->AddAction(&DragScript::OnDragOver, dropzone);
				}
			}
		}

		// 2. Handle leaving dropzones
		for (Entity last_dropzone : last_dragged_dropzones[dragging]) {
			// Rare edge case where a dropzone has its Dropzone component removed before it is
			// left. Consider what behavior makes sense here. Should OnDragLeave be dispatched
			// regardless of if the entity is still a dropzone?
			// if (!last.Has<Dropzone, Interactive>()) {
			//	continue;
			// }
			if (dragging == last_dropzone) {
				continue;
			}
			if (!over_now.contains(last_dropzone)) {
				if (auto dropzone_scripts{ last_dropzone.TryGet<Scripts>() }) {
					dropzone_scripts->AddAction(&DropzoneScript::OnDraggableLeave, dragging);
				}
				if (scripts) {
					scripts->AddAction(&DragScript::OnDragLeave, last_dropzone);
				}
			}
		}

		// 3. Always call DragOut if not currently over a dropzone
		for (Entity dropzone : dropzones) {
			if (!dropzone.Has<Dropzone, Interactive>()) {
				continue;
			}

			if (dragging == dropzone) {
				continue;
			}
			if (!over_now.contains(dropzone)) {
				if (auto dropzone_scripts{ dropzone.TryGet<Scripts>() }) {
					dropzone_scripts->AddAction(&DropzoneScript::OnDraggableOut, dragging);
				}
				if (scripts) {
					scripts->AddAction(&DragScript::OnDragOut, dropzone);
				}
			}
		}

		// Store current for next frame
		current_dragged_dropzones[dragging] = std::move(over_now);
	}

	// Update the last set
	last_dragged_dropzones = std::move(current_dragged_dropzones);
}

void SceneInput::Update(Scene& scene, const MouseInfo& mouse_state) {
	auto under_mouse{ GetEntitiesUnderMouse(scene, mouse_state) };
	auto dropzones{ GetDropzones(scene) };
	// PTGN_LOG(under_mouse.size());

	// Run mouse enter/leave logic
	UpdateMouseOverStates(under_mouse);

	// Run input event dispatch
	DispatchMouseEvents(under_mouse, mouse_state);

	// Handle drag + drop
	HandleDragging(under_mouse, dropzones, mouse_state);

	if (IsAnyDragging()) {
		HandleDropzones(under_mouse, dropzones, mouse_state);
	}

	const auto invoke_actions = [](auto& entity) {
		if (!entity.Has<Scripts>() || !entity.IsAlive()) {
			return;
		}

		auto& scripts{ entity.Get<Scripts>() };

		if (entity.Has<Interactive>()) {
			scripts.InvokeActions();
		} else {
			scripts.ClearActions();
		}
	};

	auto last{ last_mouse_over_ };
	auto dragging{ dragging_entities };

	// Save for next frame
	last_mouse_over_ = std::unordered_set(under_mouse.begin(), under_mouse.end());

	for (Entity entity : last) {
		std::invoke(invoke_actions, entity);
	}

	for (Entity entity : dropzones) {
		if (entity.Has<Dropzone>()) {
			continue;
		}
		std::invoke(invoke_actions, entity);
	}

	for (auto& [entity, _] : dragging) {
		if (entity.Has<Draggable>()) {
			continue;
		}
		std::invoke(invoke_actions, entity);
	}

	for (Entity entity : under_mouse) {
		std::invoke(invoke_actions, entity);
	}
}

// @return Null entity if entities is empty.
Entity GetTopEntity(const std::vector<Entity>& entities) {
	if (entities.empty()) {
		return {};
	}

	auto max_it{ std::max_element(entities.begin(), entities.end(), EntityDepthCompare{}) };

	return *max_it;
}

// Sets a vector to only contain its top entity.
void SetTopEntity(std::vector<Entity>& entities) {
	auto top{ GetTopEntity(entities) };
	entities.clear();
	if (top) {
		entities.emplace_back(top);
	}
}

bool IsOverlappingDropzone(
	const Entity& entity, const V2_float& world_pointer, const Entity& dropzone_entity,
	const Dropzone& dropzone
) {
	bool is_overlapping{ false };
	switch (dropzone.trigger) {
		case DropTrigger::MouseOverlaps: {
			is_overlapping = Overlap(world_pointer, dropzone_entity);
			break;
		}
		case DropTrigger::CenterOverlaps: {
			PTGN_ASSERT(
				entity.GetCamera() == dropzone_entity.GetCamera(),
				"Dropzone entity and drag entity must share the same camera"
			);
			is_overlapping = Overlap(GetAbsolutePosition(entity), dropzone_entity);
			break;
		}
		case DropTrigger::Overlaps: {
			PTGN_ASSERT(
				entity.GetCamera() == dropzone_entity.GetCamera(),
				"Dropzone entity and drag entity must share the same camera"
			);
			is_overlapping = Overlap(entity, dropzone_entity);
			break;
		}
		case DropTrigger::Contains:
			// TODO: Implement.
			PTGN_ERROR("Unimplemented drop trigger");
			break;
		default: PTGN_ERROR("Unrecognized drop trigger");
	}
	return is_overlapping;
}

std::vector<Entity> SceneInput::GetOverlappingDropzones(
	Scene& scene, const Entity& entity, const V2_float& world_pointer
) {
	std::vector<Entity> overlapping_dropzones;

	for (auto [dropzone_entity, dropzone_interactive, dropzone] :
		 scene.InternalEntitiesWith<Interactive, Dropzone>()) {
		if (IsOverlappingDropzone(entity, world_pointer, dropzone_entity, dropzone)) {
			overlapping_dropzones.emplace_back(dropzone_entity);
		}
	}

	return overlapping_dropzones;
}

void SceneInput::Init(std::size_t scene_key) {
	/*if (draw_interactives_) {
		PTGN_WARN("Drawing interactable hitboxes");
	}*/

	scene_key_ = scene_key;
	game.input.Update();
}

void SceneInput::Shutdown() {
	auto& scene{ game.scene.Get<Scene>(scene_key_) };

	top_only_		   = false;
	draw_interactives_ = true;
}

void SceneInput::SetTopOnly(bool top_only) {
	top_only_ = top_only;
}

V2_float SceneInput::TransformToCamera(const V2_float& screen_position) const {
	auto& scene{ game.scene.Get<Scene>(scene_key_) };
	if (scene.camera.primary && scene.camera.primary.Has<impl::CameraInfo>()) {
		return scene.camera.primary.TransformToCamera(screen_position);
	}
	return screen_position;
}

void SceneInput::SetDrawInteractives(bool draw_interactives) {
	draw_interactives_ = draw_interactives;
}

V2_float SceneInput::GetMousePosition() const {
	return TransformToCamera(game.input.GetMousePosition());
}

V2_float SceneInput::GetMousePositionPrevious() const {
	return TransformToCamera(game.input.GetMousePositionPrevious());
}

V2_float SceneInput::GetMouseDifference() const {
	return TransformToCamera(game.input.GetMouseDifference());
}

} // namespace ptgn