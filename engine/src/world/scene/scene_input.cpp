#include "world/scene/scene_input.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/app/application.h"
#include "core/app/manager.h"
#include "core/app/resolution.h"
#include "core/ecs/components/draw.h"
#include "core/ecs/components/interactive.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "core/input/input_handler.h"
#include "core/input/mouse.h"
#include "core/scripting/script.h"
#include "core/util/span.h"
#include "debug/core/log.h"
#include "debug/runtime/assert.h"
#include "debug/runtime/debug_system.h"
#include "math/geometry/circle.h"
#include "math/geometry/rect.h"
#include "math/overlap.h"
#include "math/vector2.h"
#include "physics/bounding_aabb.h"
#include "physics/broadphase.h"
#include "renderer/renderer.h"
#include "world/scene/camera.h"
#include "world/scene/scene.h"

namespace ptgn {

MouseInfo::MouseInfo(const Scene& scene) :
	position{ scene.input.GetMousePosition(ViewportType::World) },
	scroll_delta{ scene.input.GetMouseScroll() },
	left_pressed{ scene.input.MousePressed(Mouse::Left) },
	left_down{ scene.input.MouseDown(Mouse::Left) },
	left_up{ scene.input.MouseUp(Mouse::Left) } {}

static void GetShapes(
	const Entity& entity, const Entity& root_entity,
	std::vector<std::pair<InteractiveShape, Entity>>& vector
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
		auto interactables{ GetInteractables(entity) };
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
			!vector.empty(), "Failed to find a valid interactable for the entity: ", entity.GetId()
		);
	}
}

static Transform GetAbsoluteOffsetTransform(
	const auto& shape, const Entity& shape_entity, const Entity& parent
) {
	auto transform{ GetAbsoluteTransform(shape_entity) };

	if (parent.Has<Rect>()) {
		transform = OffsetByOrigin(parent.Get<Rect>(), transform, parent);
	}

	transform = OffsetByOrigin(shape, transform, shape_entity);
	return transform;
}

static bool Overlap(const V2_float& point, const Entity& entity) {
	std::vector<std::pair<InteractiveShape, Entity>> shapes;
	GetShapes(entity, entity, shapes);

	PTGN_ASSERT(!shapes.empty(), "Cannot check for overlap with an interactive that has no shape");

	for (const auto& [shape, e] : shapes) {
		auto transform{ GetAbsoluteOffsetTransform(shape, e, entity) };
		if (Overlap(point, transform, shape)) {
			return true;
		}
	}

	return false;
}

static bool Overlap(const Entity& entityA, const Entity& entityB) {
	std::vector<std::pair<InteractiveShape, Entity>> shapesA;
	GetShapes(entityA, entityA, shapesA);

	std::vector<std::pair<InteractiveShape, Entity>> shapesB;
	GetShapes(entityB, entityB, shapesB);

	PTGN_ASSERT(
		!shapesA.empty() && !shapesB.empty(),
		"Cannot check for overlap with an interactive that has no shape"
	);

	for (const auto& [shapeA, eA] : shapesA) {
		auto transformA{ GetAbsoluteOffsetTransform(shapeA, eA, entityA) };
		for (const auto& [shapeB, eB] : shapesB) {
			auto transformB{ GetAbsoluteOffsetTransform(shapeB, eB, entityB) };
			if (Overlap(transformA, shapeA, transformB, shapeB)) {
				return true;
			}
		}
	}

	return false;
}

bool SceneInput::IsDragging(const Entity& e) const {
	return dragging_entities_.contains(e);
}

bool SceneInput::IsAnyDragging() const {
	return !dragging_entities_.empty();
}

SceneInput::InteractiveEntities SceneInput::GetInteractiveEntities(
	Scene& scene, const MouseInfo& mouse_state
) const {
	impl::KDTree tree{ 20 };
	std::vector<impl::KDObject> objects;

	using Shapes = std::vector<std::pair<InteractiveShape, Entity>>;

	std::unordered_map<Entity, Shapes> entity_shapes;

	std::vector<Entity> all_entities;

	for (auto [entity, interactive] : scene.InternalEntitiesWith<Interactive>()) {
		if (!interactive.enabled) {
			continue;
		}
		all_entities.emplace_back(entity);
	}

	for (Entity entity : all_entities) {
		std::vector<std::pair<InteractiveShape, Entity>> shapes;

		GetShapes(entity, entity, shapes);

		entity_shapes.try_emplace(entity, shapes);

		for (const auto& [shape, shape_entity] : shapes) {
			auto transform{ GetAbsoluteOffsetTransform(shape, shape_entity, entity) };

			if (draw_interactives_) {
				auto draw_transform{ GetDrawTransform(shape_entity) };

				if (entity.Has<Rect>()) {
					draw_transform = OffsetByOrigin(entity.Get<Rect>(), draw_transform, entity);
				}

				Application::Get().debug_.DrawShape(
					draw_transform, shape, draw_interactive_color_, draw_interactive_line_width_,
					GetDrawOrigin(shape_entity), entity.GetCamera()
				);
			}

			objects.emplace_back(entity, GetBoundingAABB(shape, transform));
		}
	}
	tree.Build(objects);

	// Broadphase check.
	auto candidates{ tree.Query(mouse_state.position) };

	// PTGN_LOG("Mouse: ", mouse_state.position);

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
			if (VectorContains(entities.under_mouse, entity)) {
				continue;
			}

			auto transform{ GetAbsoluteOffsetTransform(shape, shape_entity, entity) };

			if (Overlap(mouse_state.position, transform, shape)) {
				PTGN_ASSERT(
					!VectorContains(entities.under_mouse, entity),
					"Attempting to check same interactive entity under mouse twice"
				);
				entities.under_mouse.emplace_back(entity);
			}
		}
	}

	if (top_only_ && !entities.under_mouse.empty()) {
		// Find the draggable with the highest depth.
		auto draggable_it{
			std::ranges::max_element(entities.under_mouse, EntityDepthCompare{ true })
		};

		// If no draggable is found, find the interactive entity with the highest depth.
		if (!draggable_it->Has<Draggable>()) {
			draggable_it =
				std::ranges::max_element(entities.under_mouse, EntityDepthCompare{ true });
		}

		PTGN_ASSERT(draggable_it != entities.under_mouse.end());

		entities.under_mouse = { *draggable_it };
	}
	VectorSubtract(all_entities, entities.under_mouse);
	entities.not_under_mouse = all_entities;
	return entities;
}

std::vector<Entity> SceneInput::GetDropzones(Scene& scene) {
	std::vector<Entity> objects;

	for (auto [entity, interactive, dropzone] :
		 scene.InternalEntitiesWith<Interactive, Dropzone>()) {
		if (!interactive.enabled) {
			continue;
		}

		objects.emplace_back(entity);
	}

	return objects;
}

// Called every frame
void SceneInput::UpdateMouseOverStates(const std::vector<Entity>& current) const {
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

void SceneInput::DispatchMouseEvents(
	const std::vector<Entity>& over, const std::vector<Entity>& out, const MouseInfo& mouse
) const {
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

	for (Entity e : out) {
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

bool SceneInput::IsOverlappingDropzone(
	const V2_float& mouse_position, const Entity& draggable, const Entity& dropzone,
	CallbackTrigger trigger
) {
	bool is_overlapping{ false };
	switch (trigger) {
		case CallbackTrigger::MouseOverlaps: {
			is_overlapping = Overlap(mouse_position, dropzone);
			break;
		}
		case CallbackTrigger::TransformOverlaps: {
			PTGN_ASSERT(
				draggable.GetCamera() == dropzone.GetCamera(),
				"Dropzone entity and drag entity must share the same camera"
			);
			// Origin not accounted for because this is about TransformOverlaps, not center.
			auto position{ GetAbsolutePosition(draggable) };
			is_overlapping = Overlap(position, dropzone);
			break;
		}
		case CallbackTrigger::Overlaps: {
			PTGN_ASSERT(
				draggable.GetCamera() == dropzone.GetCamera(),
				"Dropzone entity and drag entity must share the same camera"
			);
			is_overlapping = Overlap(draggable, dropzone);
			break;
		}
		case CallbackTrigger::Contains:
			// TODO: Implement.
			PTGN_ERROR("Unimplemented drop trigger");
			break;
		case CallbackTrigger::None: break;
		default:					PTGN_ERROR("Unrecognized drop trigger");
	}
	return is_overlapping;
}

void SceneInput::HandleDragging(
	const std::vector<Entity>& over, const std::vector<Entity>& dropzones, const MouseInfo& mouse
) {
	// Start dragging
	if (mouse.left_down) {
		for (Entity dragging : over) {
			if (!dragging.Has<Draggable>()) {
				continue;
			}

			if (dragging_entities_.contains(dragging)) {
				continue; // Already dragging this
			}

			dragging_entities_.emplace(dragging);

			auto scripts{ dragging.TryGet<Scripts>() };

			if (scripts) {
				scripts->AddAction(&DragScript::OnDragStart, mouse.position);
			}

			for (Entity dropzone : dropzones) {
				PTGN_ASSERT((dropzone.Has<Dropzone, Interactive>()));
				PTGN_ASSERT(dropzone.Get<Interactive>().enabled);
				if (dropzone == dragging) {
					continue;
				}

				AddDropzoneActions<DropzoneAction::Pickup>(
					dragging, dropzone, mouse.position,
					[&]() {
						dropzone.Get<Dropzone>().dropped_entities_.erase(dragging);
						if (auto dropzone_scripts{ dropzone.TryGet<Scripts>() }) {
							dropzone_scripts->AddAction(
								&DropzoneScript::OnDraggablePickup, dragging
							);
						}
					},
					[&]() {
						if (scripts) {
							scripts->AddAction(&DragScript::OnPickup, dropzone);
						}
					},
					[]() {}
				);
			}

			auto& draggable{ dragging.Get<Draggable>() };

			draggable.dragging_ = true;
			draggable.start_	= mouse.position;
			// Origin does not need to be accounted for here because offset will be used to set the
			// position (most often).
			draggable.offset_ = GetAbsolutePosition(dragging) - draggable.start_;
		}
	}

	// Continue dragging
	if (mouse.left_pressed || mouse.left_down) {
		for (Entity dragging : dragging_entities_) {
			if (!dragging.Has<Draggable>()) {
				continue;
			}
			auto scripts{ dragging.TryGet<Scripts>() };

			if (scripts) {
				scripts->AddAction(&DragScript::OnDrag);
			}
		}
	}

	// Stop dragging
	if (mouse.left_up) {
		for (Entity dragging : dragging_entities_) {
			if (!dragging.Has<Draggable>() || !dragging.Has<Interactive>() ||
				!dragging.Get<Interactive>().enabled) {
				continue;
			}

			auto scripts{ dragging.TryGet<Scripts>() };

			if (scripts) {
				scripts->AddAction(&DragScript::OnDragStop, mouse.position);
			}

			for (Entity dropzone : dropzones) {
				PTGN_ASSERT((dropzone.Has<Dropzone, Interactive>()));
				PTGN_ASSERT(dropzone.Get<Interactive>().enabled);
				if (dropzone == dragging) {
					continue;
				}

				AddDropzoneActions<DropzoneAction::Drop>(
					dragging, dropzone, mouse.position,
					[&]() {
						dropzone.Get<Dropzone>().dropped_entities_.emplace(dragging);
						if (auto dropzone_scripts{ dropzone.TryGet<Scripts>() }) {
							dropzone_scripts->AddAction(&DropzoneScript::OnDraggableDrop, dragging);
						}
					},
					[&]() {
						if (scripts) {
							scripts->AddAction(&DragScript::OnDrop, dropzone);
						}
					},
					[]() {}
				);
			}

			auto& draggable{ dragging.Get<Draggable>() };
			draggable.dragging_ = false;
			draggable.start_	= {};
			draggable.offset_	= {};
		}
		dragging_entities_.clear(); // End all drags
	}
}

void SceneInput::CleanupDropzones(const std::vector<Entity>& dropzones) {
	for (Entity dropzone : dropzones) {
		if (!dropzone.Has<Dropzone>()) {
			continue;
		}

		auto& dropped{ dropzone.Get<Dropzone>().dropped_entities_ };

		std::erase_if(dropped, [](const Entity& e) {
			return !e.IsAlive() || !e.Has<Draggable>() || !e.Has<Interactive>() ||
				   !e.Get<Interactive>().enabled;
		});
	}
}

void SceneInput::HandleDropzones(const std::vector<Entity>& dropzones, const MouseInfo& mouse) {
	// 1. Compute which dropzones each dragged entity is currently over
	for (Entity dragging : dragging_entities_) {
		if (!dragging.Has<Draggable>()) {
			continue;
		}
		auto scripts{ dragging.TryGet<Scripts>() };

		auto& draggable{ dragging.Get<Draggable>() };
		draggable.dropzones_ = {};

		for (Entity dropzone : dropzones) {
			PTGN_ASSERT((dropzone.Has<Dropzone, Interactive>()));
			PTGN_ASSERT(dropzone.Get<Interactive>().enabled);
			if (dragging == dropzone) {
				continue;
			}

			bool entered{ !draggable.last_dropzones_.contains(dropzone) };

			AddDropzoneActions<DropzoneAction::Move>(
				dragging, dropzone, mouse.position,
				[&]() {
					if (entered) {
						if (auto dropzone_scripts{ dropzone.TryGet<Scripts>() }) {
							dropzone_scripts->AddAction(
								&DropzoneScript::OnDraggableEnter, dragging
							);
							dropzone_scripts->AddAction(&DropzoneScript::OnDraggableOver, dragging);
						}
					} else {
						if (auto dropzone_scripts{ dropzone.TryGet<Scripts>() }) {
							dropzone_scripts->AddAction(&DropzoneScript::OnDraggableOver, dragging);
						}
					}
				},
				[&]() {
					if (entered) {
						if (scripts) {
							scripts->AddAction(&DragScript::OnDragEnter, dropzone);
							scripts->AddAction(&DragScript::OnDragOver, dropzone);
						}
					} else {
						if (scripts) {
							scripts->AddAction(&DragScript::OnDragOver, dropzone);
						}
					}
				},
				[&]() { draggable.dropzones_.emplace(dropzone); }
			);
		}

		// 2. Handle leaving dropzones
		for (Entity last_dropzone : draggable.last_dropzones_) {
			if (dragging == last_dropzone) {
				continue;
			}
			if (draggable.dropzones_.contains(last_dropzone)) {
				continue;
			}
			if (last_dropzone.Has<Dropzone, Interactive>() &&
				last_dropzone.Get<Interactive>().enabled) {
				if (auto dropzone_scripts{ last_dropzone.TryGet<Scripts>() }) {
					dropzone_scripts->AddAction(&DropzoneScript::OnDraggableLeave, dragging);
				}
			}
			if (scripts) {
				scripts->AddAction(&DragScript::OnDragLeave, last_dropzone);
			}
		}

		// 3. Always call DragOut if not currently over a dropzone
		for (Entity dropzone : dropzones) {
			PTGN_ASSERT((dropzone.Has<Dropzone, Interactive>()));
			PTGN_ASSERT(dropzone.Get<Interactive>().enabled);
			if (dragging == dropzone) {
				continue;
			}
			if (draggable.dropzones_.contains(dropzone)) {
				continue;
			}
			if (auto dropzone_scripts{ dropzone.TryGet<Scripts>() }) {
				dropzone_scripts->AddAction(&DropzoneScript::OnDraggableOut, dragging);
			}
			if (scripts) {
				scripts->AddAction(&DragScript::OnDragOut, dropzone);
			}
		}

		// Store current for next frame.
		draggable.last_dropzones_ = draggable.dropzones_;
	}
}

// TODO: Move to using scene engine context.

V2_float SceneInput::GetMousePosition(ViewportType relative_to, bool clamp_to_viewport) const {
	return Application::Get().input_.GetMousePosition(relative_to, clamp_to_viewport);
}

V2_float SceneInput::GetMousePositionPrevious(ViewportType relative_to, bool clamp_to_viewport)
	const {
	return Application::Get().input_.GetMousePositionPrevious(relative_to, clamp_to_viewport);
}

V2_float SceneInput::GetMousePositionDifference(ViewportType relative_to, bool clamp_to_viewport)
	const {
	return Application::Get().input_.GetMousePositionDifference(relative_to, clamp_to_viewport);
}

void SceneInput::Update(Scene& scene) {
	MouseInfo mouse_state{ scene };

	if (draw_interactives_) {
		Application::Get().debug_.DrawPoint(mouse_state.position, draw_interactive_color_);
	}

	auto entities = GetInteractiveEntities(scene, mouse_state);
	auto dropzones{ GetDropzones(scene) };
	// PTGN_LOG(under_mouse.size());

	UpdateMouseOverStates(entities.under_mouse);

	DispatchMouseEvents(entities.under_mouse, entities.not_under_mouse, mouse_state);

	HandleDragging(entities.under_mouse, dropzones, mouse_state);

	if (IsAnyDragging()) {
		HandleDropzones(dropzones, mouse_state);
	}

	// TODO: Move action invocations to separate functions:

	const auto invoke_actions = [](auto& entity) {
		if (!entity.template Has<Scripts>() || !entity.IsAlive()) {
			return;
		}

		auto& scripts{ entity.template Get<Scripts>() };

		if (entity.template Has<Interactive>() && entity.template Get<Interactive>().enabled) {
			scripts.InvokeActions();
		} else {
			scripts.ClearActions();
		}
	};

	for (Entity entity : last_mouse_over_) {
		invoke_actions(entity);
	}

	for (Entity entity : dropzones) {
		if (!entity.Has<Dropzone>()) {
			continue;
		}
		invoke_actions(entity);
	}

	for (Entity dragging : dragging_entities_) {
		if (!dragging.Has<Draggable>()) {
			continue;
		}
		invoke_actions(dragging);
	}

	for (Entity entity : entities.under_mouse) {
		invoke_actions(entity);
	}

	std::erase_if(dragging_entities_, [](const auto& entity) {
		return !entity.template Has<Draggable>();
	});

	// Save for next frame.
	last_mouse_over_ = std::unordered_set(entities.under_mouse.begin(), entities.under_mouse.end());

	CleanupDropzones(dropzones);

	scene.Refresh();
}

bool SceneInput::IsTopOnly() const {
	return top_only_;
}

void SceneInput::SetTopOnly(bool top_only) {
	top_only_ = top_only;
}

void SceneInput::SetDrawInteractives(bool draw_interactives) {
	draw_interactives_ = draw_interactives;
}

void SceneInput::SetDrawInteractivesColor(const Color& color) {
	draw_interactive_color_ = color;
}

void SceneInput::SetDrawInteractivesLineWidth(float line_width) {
	draw_interactive_line_width_ = line_width;
}

milliseconds SceneInput::GetMouseHeldTime(Mouse mouse_button) const {
	return Application::Get().input_.GetMouseHeldTime(mouse_button);
}

milliseconds SceneInput::GetKeyHeldTime(Key key) const {
	return Application::Get().input_.GetKeyHeldTime(key);
}

bool SceneInput::MouseHeld(Mouse mouse_button, milliseconds time) const {
	return Application::Get().input_.MouseHeld(mouse_button, time);
}

bool SceneInput::KeyHeld(Key key, milliseconds time) const {
	return Application::Get().input_.KeyHeld(key, time);
}

void SceneInput::SetRelativeMouseMode(bool on) const {
	Application::Get().input_.SetRelativeMouseMode(on);
}

int SceneInput::GetMouseScroll() const {
	return Application::Get().input_.GetMouseScroll();
}

bool SceneInput::MousePressed(Mouse mouse_button) const {
	return Application::Get().input_.MousePressed(mouse_button);
}

bool SceneInput::MouseReleased(Mouse mouse_button) const {
	return Application::Get().input_.MouseReleased(mouse_button);
}

bool SceneInput::MouseDown(Mouse mouse_button) const {
	return Application::Get().input_.MouseDown(mouse_button);
}

bool SceneInput::MouseUp(Mouse mouse_button) const {
	return Application::Get().input_.MouseUp(mouse_button);
}

bool SceneInput::KeyPressed(Key key) const {
	return Application::Get().input_.KeyPressed(key);
}

bool SceneInput::KeyReleased(Key key) const {
	return Application::Get().input_.KeyReleased(key);
}

bool SceneInput::KeyDown(Key key) const {
	return Application::Get().input_.KeyDown(key);
}

bool SceneInput::KeyUp(Key key) const {
	return Application::Get().input_.KeyUp(key);
}

} // namespace ptgn