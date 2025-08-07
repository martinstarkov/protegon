#include "scene/scene_input.h"

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
#include "input/input_handler.h"
#include "input/mouse.h"
#include "math/geometry.h"
#include "math/geometry/circle.h"
#include "math/geometry/rect.h"
#include "math/overlap.h"
#include "math/vector2.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_key.h"
#include "scene/scene_manager.h"
#include "utility/span.h"

// TODO: Ensure that entities are alive and interactable, etc, after each script hook is
// called in case the hook deletes or removes components from itself.
// Also check for this issue for other script hooks, e.g. buttons.

namespace ptgn {

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

bool SceneInput::PointerIsInside(
	const V2_float& screen_pointer, const V2_float& world_pointer, const Entity& entity
) const {
	// TODO: Replace with resolution.
	// if (auto window_size{ game.window.GetSize() }; !impl::OverlapPointRect(
	//		Transform{}, game.input.GetMousePositionUnclamped(), Transform{ window_size / 2.0f },
	//		Rect{ window_size }
	//	)) {
	//	// Mouse outside of screen.
	//	return false;
	//}
	return Overlap(world_pointer, entity);
}

void SceneInput::Update(Scene& scene) {
	UpdatePrevious(scene);
	UpdateCurrent(scene);
}

void SceneInput::UpdatePrevious(Scene& scene) {
	triggered_callbacks_ = false;
	for (auto [entity, interactive] : scene.InternalEntitiesWith<Interactive>()) {
		interactive.was_inside = interactive.is_inside;
		interactive.is_inside  = false;
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

void SceneInput::UpdateCurrent(Scene& scene) {
	auto screen_pointer{ game.input.GetMousePosition() };

	mouse_entered.clear();
	mouse_exited.clear();
	mouse_over.clear();
	dropzones.clear();

	bool send_mouse_event{ false };

	for (auto [entity, interactive] : scene.InternalEntitiesWith<Interactive>()) {
		auto world_pointer{ entity.GetCamera().TransformToCamera(screen_pointer) };

		if (entity.Has<Dropzone>()) {
			auto& dropzone{ entity.Get<Dropzone>() };
			for (Entity dropped_entity : dropzone.dropped_entities) {
				if (IsOverlappingDropzone(dropped_entity, world_pointer, entity, dropzone)) {
					dropzone.overlapping_draggables.emplace_back(dropped_entity);
				}
			}
			dropzones.emplace_back(entity);
		}

		bool is_inside{ PointerIsInside(screen_pointer, world_pointer, entity) };

		if (is_inside) {
			if (!interactive.was_inside) {
				mouse_entered.emplace_back(entity);
				send_mouse_event = true;
			} else {
				mouse_over.emplace_back(entity);
			}
		} else {
			if (interactive.was_inside) {
				mouse_exited.emplace_back(entity);
				send_mouse_event = true;
			}
		}

		interactive.is_inside = is_inside;
	}

	if (top_only_) {
		SetTopEntity(mouse_entered);
		SetTopEntity(mouse_over);
		SetTopEntity(dropzones);
	}

	// TODO: Fix.
	/*if (send_mouse_event) {
		OnMouseEvent(MouseEvent::Move, MouseMoveEvent{});
	}*/
}

void SceneInput::EntityMouseDown(
	Scene& scene, Entity entity, Mouse mouse, const V2_float& screen_pointer
) const {
	if (!entity.IsAlive() || !entity.Has<Interactive>()) {
		return;
	}

	InvokeScript<&impl::IScript::OnMouseDown>(entity, mouse);

	if (!entity.Has<Draggable>()) {
		return;
	}

	auto& draggable{ entity.Get<Draggable>() };

	if (draggable.dragging) {
		return;
	}

	auto world_pointer{ entity.GetCamera().TransformToCamera(screen_pointer) };

	draggable.dragging = true;
	draggable.start	   = world_pointer;
	draggable.offset   = GetAbsolutePosition(entity) - draggable.start;

	InvokeScript<&impl::IScript::OnDragStart>(entity, world_pointer);

	for (Entity dropzone_entity : dropzones) {
		if (!dropzone_entity.IsAlive() || !dropzone_entity.Has<Dropzone, Interactive>()) {
			continue;
		}
		auto& dropzone{ dropzone_entity.Get<Dropzone>() };
		bool erased{ VectorErase(dropzone.dropped_entities, entity) };
		if (erased) {
			InvokeScript<&impl::IScript::OnPickup>(entity, dropzone_entity);
		}
		// VectorErase(dropzone.overlapping_draggables, entity);
	}
}

void SceneInput::EntityMouseUp(
	Scene& scene, Entity entity, bool is_inside, Mouse mouse, const V2_float& screen_pointer
) const {
	if (!entity.IsAlive() || !entity.Has<Interactive>()) {
		return;
	}

	if (is_inside) {
		InvokeScript<&impl::IScript::OnMouseUp>(entity, mouse);
	} else {
		InvokeScript<&impl::IScript::OnMouseUpOutside>(entity, mouse);
	}

	if (!entity.Has<Draggable>()) {
		return;
	}

	auto& draggable{ entity.Get<Draggable>() };

	if (!draggable.dragging) {
		return;
	}

	auto world_pointer{ entity.GetCamera().TransformToCamera(screen_pointer) };

	draggable.dragging = false;
	draggable.offset   = {};

	InvokeScript<&impl::IScript::OnDragStop>(entity, world_pointer);

	auto overlapping_dropzones{ GetOverlappingDropzones(scene, entity, world_pointer) };

	if (top_only_) {
		SetTopEntity(overlapping_dropzones);
	}

	for (const auto& dropzone : overlapping_dropzones) {
		if (!dropzone.IsAlive() || !dropzone.Has<Dropzone, Interactive>()) {
			continue;
		}
		InvokeScript<&impl::IScript::OnDrop>(entity, dropzone);
		// TODO: OnDrop may delete the dropzone, which would invalidate this:
		auto& dropzone_entities{ dropzone.Get<Dropzone>().dropped_entities };
		if (!VectorContains(dropzone_entities, entity)) {
			dropzone_entities.emplace_back(entity);
		}
	}
}

// TODO: Move callback processing to a second step.
void SceneInput::EntityMouseMove(
	Scene& scene, Entity entity, bool is_inside, bool was_inside, const V2_float& screen_pointer
) const {
	if (!entity.IsAlive() || !entity.Has<Interactive>()) {
		return;
	}

	auto world_pointer{ entity.GetCamera().TransformToCamera(screen_pointer) };

	InvokeScript<&impl::IScript::OnMouseMove>(entity, world_pointer);

	bool entered{ is_inside && !was_inside };
	bool exited{ !is_inside && was_inside };

	if (entered) {
		InvokeScript<&impl::IScript::OnMouseEnter>(entity, world_pointer);
	}
	if (exited) {
		InvokeScript<&impl::IScript::OnMouseLeave>(entity, world_pointer);
	}

	if (is_inside) {
		InvokeScript<&impl::IScript::OnMouseOver>(entity, world_pointer);
	} else {
		InvokeScript<&impl::IScript::OnMouseOut>(entity, world_pointer);
	}

	if (entity.Has<Draggable>() && entity.Get<Draggable>().dragging) {
		// Dragging.
		// TODO: Move this to a dragging system.
		PTGN_ASSERT(!entity.Has<Dropzone>(), "Draggable entity cannot also be a dropzone");
		InvokeScript<&impl::IScript::OnDrag>(entity, world_pointer);
	}
}

void SceneInput::ProcessDragOverDropzones(Scene& scene, const V2_float& screen_pointer) const {
	for (const auto& [draggable_entity, interactive1, draggable] :
		 scene.InternalEntitiesWith<Interactive, Draggable>()) {
		// Not dragging currently.
		if (!draggable.dragging) {
			continue;
		}
		// Mouse not moving over the draggable (ensures that draggable is top entity if applicable).
		if (!VectorContains(mouse_over, draggable_entity) &&
			!VectorContains(mouse_entered, draggable_entity)) {
			continue;
		}

		auto world_pointer{ draggable_entity.GetCamera().TransformToCamera(screen_pointer) };
		for (Entity dropzone_entity : dropzones) {
			if (!dropzone_entity.IsAlive() || !dropzone_entity.Has<Dropzone, Interactive>()) {
				continue;
			}
			auto& dropzone{ dropzone_entity.Get<Dropzone>() };
			bool was_overlapping{
				VectorContains(dropzone.overlapping_draggables, draggable_entity)
			};
			bool is_overlapping{
				IsOverlappingDropzone(draggable_entity, world_pointer, dropzone_entity, dropzone)
			};
			if (is_overlapping) {
				if (!was_overlapping) {
					InvokeScript<&impl::IScript::OnDragEnter>(draggable_entity, dropzone_entity);
					dropzone.overlapping_draggables.emplace_back(draggable_entity);
				} else {
					InvokeScript<&impl::IScript::OnDragOver>(draggable_entity, dropzone_entity);
				}
			} else {
				VectorErase(dropzone.overlapping_draggables, draggable_entity);
				if (!was_overlapping) {
					InvokeScript<&impl::IScript::OnDragOut>(draggable_entity, dropzone_entity);
				} else {
					InvokeScript<&impl::IScript::OnDragLeave>(draggable_entity, dropzone_entity);
				}
			}
		}
	}
}

// TODO: Fix.
/*
void SceneInput::OnMouseEvent(MouseEvent type, const Event& event) {
	// TODO: Figure out a smart way to cache the scene.
	if (!game.scene.HasScene(scene_key_)) {
		return;
	}
	auto& scene{ game.scene.Get<Scene>(scene_key_) };
	auto screen_pointer{ game.input.GetMousePosition() };
	switch (type) {
		case MouseEvent::Move: {
			// Prevent mouse move event from triggering twice per frame.
			if (triggered_callbacks_) {
				return;
			} else {
				triggered_callbacks_ = true;
			}
			for (Entity entity : mouse_entered) {
				EntityMouseMove(scene, entity, true, false, screen_pointer);
			}
			for (Entity entity : mouse_over) {
				EntityMouseMove(scene, entity, true, true, screen_pointer);
			}
			for (Entity entity : mouse_exited) {
				EntityMouseMove(scene, entity, false, true, screen_pointer);
			}
			ProcessDragOverDropzones(scene, screen_pointer);
			break;
		}
		case MouseEvent::Down: {
			Mouse mouse{ static_cast<const MouseDownEvent&>(event).mouse };
			for (Entity entity : mouse_entered) {
				EntityMouseDown(scene, entity, mouse, screen_pointer);
			}
			for (Entity entity : mouse_over) {
				EntityMouseDown(scene, entity, mouse, screen_pointer);
			}
			for (auto [entity, interactive] : scene.InternalEntitiesWith<Interactive>()) {
				if (interactive.is_inside || !entity.IsAlive() ||
					!entity.Has<Interactive>()) {
					continue;
				}
				entity.InvokeScript<&impl::IScript::OnMouseDownOutside>(mouse);
			}
			break;
		}
		case MouseEvent::Up: {
			Mouse mouse{ static_cast<const MouseUpEvent&>(event).mouse };
			for (auto [entity, interactive] : scene.InternalEntitiesWith<Interactive>()) {
				EntityMouseUp(scene, entity, interactive.is_inside, mouse, screen_pointer);
			}
			break;
		}
		case MouseEvent::Pressed: {
			Mouse mouse{ static_cast<const MousePressedEvent&>(event).mouse };
			for (Entity entity : mouse_entered) {
				if (!entity.IsAlive() || !entity.Has<Interactive>()) {
					continue;
				}
				entity.InvokeScript<&impl::IScript::OnMousePressed>(mouse);
			}
			for (Entity entity : mouse_over) {
				if (!entity.IsAlive() || !entity.Has<Interactive>()) {
					continue;
				}
				entity.InvokeScript<&impl::IScript::OnMousePressed>(mouse);
			}
			break;
		}
		case MouseEvent::Scroll: {
			V2_int scroll{ static_cast<const MouseScrollEvent&>(event).scroll };
			for (Entity entity : mouse_entered) {
				if (!entity.IsAlive() || !entity.Has<Interactive>()) {
					continue;
				}
				entity.InvokeScript<&impl::IScript::OnMouseScroll>(scroll);
			}
			for (Entity entity : mouse_over) {
				if (!entity.IsAlive() || !entity.Has<Interactive>()) {
					continue;
				}
				entity.InvokeScript<&impl::IScript::OnMouseScroll>(scroll);
			}
			break;
		}
		default: PTGN_ERROR("Unimplemented mouse event type");
	}
}

void SceneInput::OnKeyEvent(KeyEvent type, const Event& event) {
	if (!game.scene.HasScene(scene_key_)) {
		return;
	}
	auto& scene{ game.scene.Get<Scene>(scene_key_) };
	switch (type) {
		case KeyEvent::Down: {
			Key key{ static_cast<const KeyDownEvent&>(event).key };
			for (auto [entity, interactive, scripts] :
				 scene.InternalEntitiesWith<Interactive, Scripts>()) {
				entity.InvokeScript<&impl::IScript::OnKeyDown>(key);
			}
			break;
		}
		case KeyEvent::Up: {
			Key key{ static_cast<const KeyUpEvent&>(event).key };
			for (auto [entity, interactive, scripts] :
				 scene.InternalEntitiesWith<Interactive, Scripts>()) {
				entity.InvokeScript<&impl::IScript::OnKeyUp>(key);
			}
			break;
		}
		case KeyEvent::Pressed: {
			Key key{ static_cast<const KeyPressedEvent&>(event).key };
			for (auto [entity, interactive, scripts] :
				 scene.InternalEntitiesWith<Interactive, Scripts>()) {
				entity.InvokeScript<&impl::IScript::OnKeyPressed>(key);
			}
			break;
		}
		default: PTGN_ERROR("Unimplemented key event type");
	}
}*/

void SceneInput::ResetInteractives(Scene& scene) {
	for (auto [entity, interactive] : scene.InternalEntitiesWith<Interactive>()) {
		interactive.was_inside = false;
		interactive.is_inside  = false;
	}
}

void SceneInput::Init(std::size_t scene_key) {
	/*if (draw_interactives_) {
		PTGN_WARN("Drawing interactable hitboxes");
	}*/

	scene_key_ = scene_key;
	// Input is reset to ensure no previously pressed keys are considered held.
	// TODO: Check if this is necessary or not.
	// game.input.ResetKeyStates();
	// game.input.ResetMouseStates();
	game.input.Update();

	auto& scene{ game.scene.Get<Scene>(scene_key_) };

	ResetInteractives(scene);
	UpdateCurrent(scene);
	// TODO: Fix.
	// OnMouseEvent(MouseEvent::Move, MouseMoveEvent{});

	// TODO: Cache interactive entity list every frame to avoid repeated calls for each
	// mouse and keyboard event type.

	// TODO: Fix.
	/*game.event.key.Subscribe(
		this, std::bind(&SceneInput::OnKeyEvent, this, std::placeholders::_1, std::placeholders::_2)
	);

	game.event.mouse.Subscribe(
		this,
		std::bind(&SceneInput::OnMouseEvent, this, std::placeholders::_1, std::placeholders::_2)
	);*/
}

void SceneInput::Shutdown() {
	auto& scene{ game.scene.Get<Scene>(scene_key_) };
	ResetInteractives(scene);

	triggered_callbacks_ = false;
	top_only_			 = false;
	draw_interactives_	 = true;
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

void SceneInput::SimulateMouseMovement(Entity entity) {
	if (!IsInteractive(entity)) {
		return;
	}
	PTGN_ASSERT(entity.Has<impl::SceneKey>());
	const auto& scene_key{ entity.Get<impl::SceneKey>() };
	if (!game.scene.HasScene(scene_key)) {
		return;
	}
	auto& scene{ game.scene.Get<Scene>(scene_key) };
	impl::SetInteractiveWasInside(entity, false);
	auto screen_pointer{ game.input.GetMousePosition() };
	scene.input.EntityMouseMove(
		scene, entity, impl::InteractiveIsInside(entity), impl::InteractiveWasInside(entity),
		screen_pointer
	);
}

} // namespace ptgn