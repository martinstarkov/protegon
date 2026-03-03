#include "runtime/scene/scene_input.h"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "app/context.h"
#include "core/assert.h"
#include "core/log.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/overlap.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/util/span.h"
#include "platform/input/input_handler.h"
#include "platform/input/key.h"
#include "platform/input/mouse.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/shape.h"
#include "runtime/physics/bounding_aabb.h"
#include "runtime/physics/broadphase.h"
#include "runtime/scene/resolution.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/scripts.h"
#include "runtime/ui/interactive.h"

// TODO: Actually implement Draggable enabled boolean (currently it does nothing).
// TODO: Actually implement Dropzone enabled boolean (currently it does nothing).

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

static Transform GetWorldOffsetTransform(const auto& shape, Entity shape_entity, Entity parent) {
	auto transform{ GetWorldTransform(shape_entity) };

	if (parent.Has<Rect>()) {
		transform = OffsetByOrigin(parent.Get<Rect>(), transform, parent);
	}

	transform = OffsetByOrigin(shape, transform, shape_entity);
	return transform;
}

static bool Overlap(V2_float point, Entity entity) {
	std::vector<std::pair<InteractiveShape, Entity>> shapes;
	GetShapes(entity, entity, shapes);

	PTGN_ASSERT(!shapes.empty(), "Cannot check for overlap with an interactive that has no shape");

	for (const auto& [shape, e] : shapes) {
		auto transform{ GetWorldOffsetTransform(shape, e, entity) };
		if (Overlap(point, transform, shape)) {
			return true;
		}
	}

	return false;
}

static bool Overlap(Entity entityA, Entity entityB) {
	std::vector<std::pair<InteractiveShape, Entity>> shapesA;
	GetShapes(entityA, entityA, shapesA);

	std::vector<std::pair<InteractiveShape, Entity>> shapesB;
	GetShapes(entityB, entityB, shapesB);

	PTGN_ASSERT(
		!shapesA.empty() && !shapesB.empty(),
		"Cannot check for overlap with an interactive that has no shape"
	);

	for (const auto& [shapeA, eA] : shapesA) {
		auto transformA{ GetWorldOffsetTransform(shapeA, eA, entityA) };
		for (const auto& [shapeB, eB] : shapesB) {
			auto transformB{ GetWorldOffsetTransform(shapeB, eB, entityB) };
			if (Overlap(transformA, shapeA, transformB, shapeB)) {
				return true;
			}
		}
	}

	return false;
}

namespace impl {

MouseInfo::MouseInfo(const Scene& scene) :
	// TODO: Change to window once renderer uses correct viewport system.
	position{ scene.input.GetMousePosition(Frame::Window) },
	scroll_delta{ scene.input.GetMouseScroll() },
	left_held{ scene.input.MouseHeld(Mouse::Left) },
	left_pressed{ scene.input.MousePressed(Mouse::Left) },
	left_released{ scene.input.MouseReleased(Mouse::Left) } {}

} // namespace impl

SceneInput::SceneInput(Scene& scene) : scene_{ scene } {}

bool SceneInput::IsAnyDragging() const {
	return !dragging_entities_.empty();
}

bool SceneInput::IsTopOnly() const {
	return top_only_;
}

void SceneInput::SetTopOnly(bool top_only) {
	top_only_ = top_only;
}

void SceneInput::SetInteractiveDebugDraw(const InteractiveDebugDrawSettings& settings) {
	interactive_debug_draw_settings_ = settings;
}

V2_float SceneInput::GetMousePosition(Frame position_frame_of_reference, bool clamp_to_viewport)
	const {
	auto position{ ctx_->input.GetMousePosition() };

	if (!clamp_to_viewport) {
		position = ctx_->input.GetMouseScreenPosition();
	}

	return GetMousePositionRelativeTo(position, position_frame_of_reference, clamp_to_viewport);
}

V2_float SceneInput::GetPreviousMousePosition(
	Frame position_frame_of_reference, bool clamp_to_viewport
) const {
	return GetMousePositionRelativeTo(
		ctx_->input.GetPreviousMousePosition(), position_frame_of_reference, clamp_to_viewport
	);
}

V2_float SceneInput::GetMouseDelta(Frame delta_frame_of_reference, bool clamp_to_viewport) const {
	return GetMousePosition(delta_frame_of_reference, clamp_to_viewport) -
		   GetPreviousMousePosition(delta_frame_of_reference, clamp_to_viewport);
}

float SceneInput::GetMouseScroll() const {
	return ctx_->input.GetMouseScroll();
}

bool SceneInput::MousePressed(Mouse button) const {
	return ctx_->input.MousePressed(button);
}

bool SceneInput::MouseReleased(Mouse button) const {
	return ctx_->input.MouseReleased(button);
}

bool SceneInput::MouseHeld(Mouse button) const {
	return ctx_->input.MouseHeld(button);
}

bool SceneInput::MouseHeld(Mouse button, milliseconds time) const {
	return ctx_->input.MouseHeld(button, time);
}

milliseconds SceneInput::GetMouseHeldTime(Mouse button) const {
	return ctx_->input.GetMouseHeldTime(button);
}

bool SceneInput::KeyPressed(Key key) const {
	return ctx_->input.KeyPressed(key);
}

bool SceneInput::KeyReleased(Key key) const {
	return ctx_->input.KeyReleased(key);
}

bool SceneInput::KeyHeld(Key key) const {
	return ctx_->input.KeyHeld(key);
}

milliseconds SceneInput::GetKeyHeldTime(Key key) const {
	return ctx_->input.GetKeyHeldTime(key);
}

void SceneInput::Init(const std::shared_ptr<ApplicationContext>& ctx) {
	ctx_ = ctx;
}

V2_float SceneInput::GetMousePositionRelativeTo(
	V2_float position, Frame position_frame_of_reference, bool clamp_to_viewport
) const {
	return ConvertPoint(
		position, Frame::Window, position_frame_of_reference, FrameContext{ scene_ }
	);
}

SceneInput::InteractiveEntities SceneInput::GetInteractiveEntities(
	const impl::MouseInfo& mouse_state
) const {
	impl::KDTree tree{ 20 };
	std::vector<impl::KDObject> objects;

	std::unordered_map<Entity, std::vector<std::pair<InteractiveShape, Entity>>> entity_shapes;

	std::vector<Entity> all_entities;

	for (auto [entity, interactive] : scene_.EntitiesWith<impl::Interactive>()) {
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
			auto transform{ GetWorldOffsetTransform(shape, shape_entity, entity) };

			if (interactive_debug_draw_settings_.enabled) {
				auto draw_transform{ GetDrawTransform(shape_entity) };

				if (entity.Has<Rect>()) {
					draw_transform = OffsetByOrigin(entity.Get<Rect>(), draw_transform, entity);
				}

				// TODO: Use debub shape draw.
				impl::DrawShape(
					ctx_->renderer, shape, draw_transform, interactive_debug_draw_settings_.color,
					interactive_debug_draw_settings_.line_width, GetDrawOrigin(shape_entity),
					GetDepth(shape_entity), GetBlendMode(shape_entity)
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

			auto transform{ GetWorldOffsetTransform(shape, shape_entity, entity) };

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
			std::ranges::max_element(entities.under_mouse, impl::EntityDepthCompare{ true })
		};

		// If no draggable is found, find the interactive entity with the highest depth.
		if (!draggable_it->Has<impl::Draggable>()) {
			draggable_it =
				std::ranges::max_element(entities.under_mouse, impl::EntityDepthCompare{ true });
		}

		PTGN_ASSERT(draggable_it != entities.under_mouse.end());

		entities.under_mouse = { *draggable_it };
	}
	VectorSubtract(all_entities, entities.under_mouse);
	entities.not_under_mouse = all_entities;
	return entities;
}

std::vector<Entity> SceneInput::GetDropzones() {
	std::vector<Entity> objects;

	for (auto [entity, interactive, dropzone] :
		 scene_.EntitiesWith<impl::Interactive, impl::Dropzone>()) {
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
		if (!e.Has<impl::Scripts>()) {
			continue;
		}
		if (!last_mouse_over_.contains(e)) {
			MouseEnter event;
			e.Get<impl::Scripts>().Emit(event);
		}
	}

	for (Entity e : last_mouse_over_) {
		if (!e.Has<impl::Scripts>()) {
			continue;
		}
		if (!VectorContains(current, e)) {
			MouseLeave event;
			e.Get<impl::Scripts>().Emit(event);
		}
	}
}

void SceneInput::DispatchMouseEvents(
	const std::vector<Entity>& over, const std::vector<Entity>& out, const impl::MouseInfo& mouse
) const {
	for (Entity e : over) {
		if (!e.Has<impl::Scripts>()) {
			continue;
		}

		MouseMoveOver move_over_event;
		e.Get<impl::Scripts>().Emit(move_over_event);

		if (mouse.left_pressed) {
			MousePressedOver event;
			event.button = Mouse::Left;
			e.Get<impl::Scripts>().Emit(event);
		}
		if (mouse.left_held) {
			MouseHeldOver event;
			event.button = Mouse::Left;
			e.Get<impl::Scripts>().Emit(event);
		}
		if (mouse.left_released) {
			MouseReleasedOver event;
			event.button = Mouse::Left;
			e.Get<impl::Scripts>().Emit(event);
		}
		if (!mouse.scroll_delta.IsZero()) {
			MouseScrollOver event;
			event.scroll_delta = mouse.scroll_delta;
			e.Get<impl::Scripts>().Emit(event);
		}
	}

	for (Entity e : out) {
		if (!e.Has<impl::Scripts>()) {
			continue;
		}
		if (VectorContains(over, e)) {
			continue;
		}

		MouseMoveOut mouse_move_out{};
		e.Get<impl::Scripts>().Emit(mouse_move_out);

		if (mouse.left_pressed) {
			MousePressedOut event;
			event.button = Mouse::Left;
			e.Get<impl::Scripts>().Emit(event);
		}
		if (mouse.left_held) {
			MouseHeldOut event;
			event.button = Mouse::Left;
			e.Get<impl::Scripts>().Emit(event);
		}
		if (mouse.left_released) {
			MouseReleasedOut event;
			event.button = Mouse::Left;
			e.Get<impl::Scripts>().Emit(event);
		}
		if (!mouse.scroll_delta.IsZero()) {
			MouseScrollOut event;
			event.scroll_delta = mouse.scroll_delta;
			e.Get<impl::Scripts>().Emit(event);
		}
	}
}

bool SceneInput::IsOverlappingDropzone(
	const V2_float& mouse_position, const Entity& draggable, const Entity& dropzone,
	TriggerCondition condition
) {
	bool is_overlapping{ false };
	switch (condition) {
		using enum ptgn::TriggerCondition;
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

void SceneInput::HandleDragging(
	const std::vector<Entity>& over, const std::vector<Entity>& dropzones,
	const impl::MouseInfo& mouse
) {
	// Start dragging
	if (mouse.left_pressed) {
		for (Entity dragging : over) {
			if (!dragging.Has<impl::Draggable>()) {
				continue;
			}

			if (dragging_entities_.contains(dragging)) {
				continue; // Already dragging this
			}

			dragging_entities_.emplace(dragging);

			if (auto scripts{ dragging.TryGet<impl::Scripts>() }) {
				DragStart event;
				event.start_position = mouse.position;
				scripts->Emit(event);
			}

			if (!dragging.Has<impl::Draggable>()) {
				continue;
			}

			for (Entity dropzone : dropzones) {
				PTGN_ASSERT((dropzone.Has<impl::Dropzone, impl::Interactive>()));
				PTGN_ASSERT(dropzone.Get<impl::Interactive>().enabled);
				if (dropzone == dragging) {
					continue;
				}

				AddDropzoneActions<DropzoneAction::Pickup>(
					dragging, dropzone, mouse.position,
					[&]() {
						dropzone.Get<impl::Dropzone>().draggables.erase(dragging);
						if (auto dropzone_scripts{ dropzone.TryGet<impl::Scripts>() }) {
							PickupFromDropzone event;
							event.draggable = dragging;
							dropzone_scripts->Emit(event);
						}
					},
					[&]() {
						if (auto scripts{ dragging.TryGet<impl::Scripts>() }) {
							PickupDraggable event;
							event.dropzone = dropzone;
							scripts->Emit(event);
						}
					},
					[]() {}
				);
			}

			if (!dragging.Has<impl::Draggable>()) {
				continue;
			}

			auto& draggable{ dragging.Get<impl::Draggable>() };

			draggable.dragging = true;
			draggable.start	   = mouse.position;
			// Origin does not need to be accounted for here because offset will be used to set the
			// position (most often).
			draggable.offset = GetWorldTransform(dragging).GetPosition() - draggable.start;
		}
	}

	// Continue dragging
	if (mouse.left_held || mouse.left_pressed) {
		for (Entity dragging : dragging_entities_) {
			if (!dragging.Has<impl::Draggable>()) {
				continue;
			}

			if (auto scripts{ dragging.TryGet<impl::Scripts>() }) {
				Dragging event;
				event.offset = dragging.Get<impl::Draggable>().offset;
				scripts->Emit(event);
			}
		}
	}

	// Stop dragging
	if (mouse.left_released) {
		for (Entity dragging : dragging_entities_) {
			if (!dragging.Has<impl::Draggable>() || !dragging.Has<impl::Interactive>() ||
				!dragging.Get<impl::Interactive>().enabled) {
				continue;
			}

			if (auto scripts{ dragging.TryGet<impl::Scripts>() }) {
				DragStop event;
				event.stop_position = mouse.position;
				scripts->Emit(event);
			}

			if (!dragging.Has<impl::Draggable>() || !dragging.Has<impl::Interactive>() ||
				!dragging.Get<impl::Interactive>().enabled) {
				continue;
			}

			for (Entity dropzone : dropzones) {
				PTGN_ASSERT((dropzone.Has<impl::Dropzone, impl::Interactive>()));
				PTGN_ASSERT(dropzone.Get<impl::Interactive>().enabled);
				if (dropzone == dragging) {
					continue;
				}

				AddDropzoneActions<DropzoneAction::Drop>(
					dragging, dropzone, mouse.position,
					[&]() {
						dropzone.Get<impl::Dropzone>().draggables.emplace(dragging);
						if (auto dropzone_scripts{ dropzone.TryGet<impl::Scripts>() }) {
							DropIntoDropzone event;
							event.draggable = dragging;
							dropzone_scripts->Emit(event);
						}
					},
					[&]() {
						if (auto scripts{ dragging.TryGet<impl::Scripts>() }) {
							DropDraggable event;
							event.dropzone = dropzone;
							scripts->Emit(event);
						}
					},
					[]() {}
				);
			}

			if (!dragging.Has<impl::Draggable>() || !dragging.Has<impl::Interactive>() ||
				!dragging.Get<impl::Interactive>().enabled) {
				continue;
			}

			auto& draggable{ dragging.Get<impl::Draggable>() };
			draggable.dragging = false;
			draggable.start	   = {};
			draggable.offset   = {};
		}
		dragging_entities_.clear(); // End all drags
	}
}

void SceneInput::CleanupDropzones(const std::vector<Entity>& dropzones) {
	for (Entity dropzone : dropzones) {
		if (!dropzone.Has<impl::Dropzone>()) {
			continue;
		}

		auto& dropped{ dropzone.Get<impl::Dropzone>().draggables };

		std::erase_if(dropped, [](const Entity& e) {
			return !e || !e.Has<impl::Draggable>() || !e.Has<impl::Interactive>() ||
				   !e.Get<impl::Interactive>().enabled;
		});
	}
}

void SceneInput::HandleDropzones(
	const std::vector<Entity>& dropzones, const impl::MouseInfo& mouse
) {
	// 1. Compute which dropzones each dragged entity is currently over
	for (Entity dragging : dragging_entities_) {
		if (!dragging.Has<impl::Draggable>()) {
			continue;
		}

		auto& draggable{ dragging.Get<impl::Draggable>() };
		draggable.dropzones = {};

		for (Entity dropzone : dropzones) {
			PTGN_ASSERT((dropzone.Has<impl::Dropzone, impl::Interactive>()));
			PTGN_ASSERT(dropzone.Get<impl::Interactive>().enabled);
			if (dragging == dropzone) {
				continue;
			}

			bool entered{ !draggable.last_dropzones.contains(dropzone) };

			AddDropzoneActions<DropzoneAction::Move>(
				dragging, dropzone, mouse.position,
				[&]() {
					if (entered) {
						if (auto dropzone_scripts{ dropzone.TryGet<impl::Scripts>() }) {
							EnterDropzone event1;
							event1.draggable = dragging;
							dropzone_scripts->Emit(event1);
							MoveOverDropzone event2;
							event2.draggable = dragging;
							dropzone_scripts->Emit(event2);
						}
					} else {
						if (auto dropzone_scripts{ dropzone.TryGet<impl::Scripts>() }) {
							MoveOverDropzone event2;
							event2.draggable = dragging;
							dropzone_scripts->Emit(event2);
						}
					}
				},
				[&]() {
					if (entered) {
						if (auto scripts{ dragging.TryGet<impl::Scripts>() }) {
							DragEnter event1;
							event1.dropzone = dropzone;
							scripts->Emit(event1);
							DragOver event2;
							event2.dropzone = dropzone;
							scripts->Emit(event2);
						}
					} else {
						if (auto scripts{ dragging.TryGet<impl::Scripts>() }) {
							DragOver event2;
							event2.dropzone = dropzone;
							scripts->Emit(event2);
						}
					}
				},
				[&]() { draggable.dropzones.emplace(dropzone); }
			);
		}

		if (!dragging.Has<impl::Draggable>()) {
			continue;
		}

		// 2. Handle leaving dropzones
		for (Entity last_dropzone : draggable.last_dropzones) {
			if (dragging == last_dropzone) {
				continue;
			}
			if (draggable.dropzones.contains(last_dropzone)) {
				continue;
			}
			if (last_dropzone.Has<impl::Dropzone, impl::Interactive>() &&
				last_dropzone.Get<impl::Interactive>().enabled) {
				if (auto dropzone_scripts{ last_dropzone.TryGet<impl::Scripts>() }) {
					LeaveDropzone event;
					event.draggable = dragging;
					dropzone_scripts->Emit(event);
				}
			}
			if (auto scripts{ dragging.TryGet<impl::Scripts>() }) {
				DragLeave event;
				event.last_dropzone = last_dropzone;
				scripts->Emit(event);
			}
		}

		if (!dragging.Has<impl::Draggable>()) {
			continue;
		}

		// 3. Always call DragOut if not currently over a dropzone
		for (Entity dropzone : dropzones) {
			PTGN_ASSERT((dropzone.Has<impl::Dropzone, impl::Interactive>()));
			PTGN_ASSERT(dropzone.Get<impl::Interactive>().enabled);
			if (dragging == dropzone) {
				continue;
			}
			if (draggable.dropzones.contains(dropzone)) {
				continue;
			}
			if (auto dropzone_scripts{ dropzone.TryGet<impl::Scripts>() }) {
				MoveOutsideDropzone event;
				event.draggable = dragging;
				dropzone_scripts->Emit(event);
			}
			if (auto scripts{ dragging.TryGet<impl::Scripts>() }) {
				DragOut event;
				event.dropzone = dropzone;
				scripts->Emit(event);
			}
		}

		if (!dragging.Has<impl::Draggable>()) {
			continue;
		}

		// Store current for next frame.
		draggable.last_dropzones = draggable.dropzones;
	}
}

void SceneInput::Update() {
	impl::MouseInfo mouse_state{ scene_ };

	if (interactive_debug_draw_settings_.enabled) {
		// TODO: Use debub shape draw.
		impl::DrawShape(
			ctx_->renderer, V2_float{ mouse_state.position }, Transform{},
			interactive_debug_draw_settings_.color, FillStyle{}, Origin::Center, 0, BlendMode::Blend
		);
	}

	auto entities = GetInteractiveEntities(mouse_state);
	auto dropzones{ GetDropzones() };
	// PTGN_LOG(entities);

	UpdateMouseOverStates(entities.under_mouse);

	DispatchMouseEvents(entities.under_mouse, entities.not_under_mouse, mouse_state);

	HandleDragging(entities.under_mouse, dropzones, mouse_state);

	if (IsAnyDragging()) {
		HandleDropzones(dropzones, mouse_state);
	}

	// TODO: Move action invocations to separate functions:

	std::erase_if(dragging_entities_, [](const auto& entity) {
		return !entity.template Has<impl::Draggable>();
	});

	// Save for next frame.
	last_mouse_over_ = std::unordered_set(entities.under_mouse.begin(), entities.under_mouse.end());

	CleanupDropzones(dropzones);

	scene_.Refresh();
}

} // namespace ptgn