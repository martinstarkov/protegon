#include "scene/scene_input.h"

#include <functional>
#include <vector>

#include "common/assert.h"
#include "components/common.h"
#include "components/input.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/window.h"
#include "debug/log.h"
#include "events/event.h"
#include "events/event_handler.h"
#include "events/events.h"
#include "input/input_handler.h"
#include "input/key.h"
#include "input/mouse.h"
#include "math/geometry/circle.h"
#include "math/geometry/rect.h"
#include "math/overlap.h"
#include "math/vector2.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_key.h"
#include "scene/scene_manager.h"

// TODO: Clean up this mess of a file.

namespace ptgn {

std::vector<std::pair<Shape, Entity>> GetShapes(const Entity& entity) {
	if (entity.Has<Rect>()) {
		const auto& rect{ entity.Get<Rect>() };
		auto offset_rect{ ApplyOffset(rect, entity) };
		return { { offset_rect, entity } };
	} else if (entity.Has<Circle>()) {
		return { { entity.Get<Circle>(), entity } };
	} else if (entity.Has<TextureHandle>()) {
		Rect texture_rect{ entity.Get<TextureHandle>().GetSize() };
		auto offset_rect{ ApplyOffset(texture_rect, entity) };
		return { { offset_rect, entity } };
	}
	std::vector<std::pair<Shape, Entity>> vector;
	auto children{ entity.GetChildren() };
	for (const auto& child : children) {
		auto shapes{ GetShapes(child) };
		vector.insert(vector.end(), shapes.begin(), shapes.end());
	}
	return {};
}

bool Overlap(const V2_float& point, const Entity& entity) {
	auto shapes{ GetShapes(entity) };
	PTGN_ASSERT(!shapes.empty(), "Cannot check for overlap with an interactive that has no shape");
	for (const auto& [shape, e] : shapes) {
		auto transform{ e.GetAbsoluteTransform() };
		if (Overlap(point, transform, shape)) {
			return true;
		}
	}
	return false;
}

bool Overlap(const Entity& entityA, const Entity& entityB) {
	auto shapesA{ GetShapes(entityA) };
	auto shapesB{ GetShapes(entityB) };
	PTGN_ASSERT(
		!shapesA.empty() && !shapesB.empty(),
		"Cannot check for overlap with an interactive that has no shape"
	);
	for (const auto& [shapeA, eA] : shapesA) {
		auto transformA{ eA.GetAbsoluteTransform() };
		for (const auto& [shapeB, eB] : shapesB) {
			auto transformB{ eB.GetAbsoluteTransform() };
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
	auto window_size{ game.window.GetSize() };
	if (!impl::OverlapPointRect(Transform{}, screen_pointer, Transform{}, Rect{ window_size })) {
		// Mouse outside of screen.
		return false;
	}
	return Overlap(world_pointer, entity);
}

void SceneInput::Update(Scene& scene) {
	UpdatePrevious(scene);
	UpdateCurrent(scene);
}

void SceneInput::UpdatePrevious(Scene& scene) {
	triggered_callbacks_ = false;
	for (auto [entity, enabled, interactive] : scene.EntitiesWith<Enabled, Interactive>()) {
		if (!enabled) {
			continue;
		}
		interactive.was_inside = interactive.is_inside;
		interactive.is_inside  = false;
	}
}

void SceneInput::UpdateCurrent(Scene& scene) {
	// auto mouse_pos{ game.input.GetMousePosition() };
	auto screen_pointer{ game.input.GetMousePositionUnclamped() };

	Depth top_depth;
	Entity top_entity;

	bool send_mouse_event{ false };
	for (auto [entity, enabled, interactive] : scene.EntitiesWith<Enabled, Interactive>()) {
		if (!enabled) {
			interactive.is_inside  = false;
			interactive.was_inside = false;
		} else {
			auto world_pointer{ entity.GetCamera().TransformToCamera(screen_pointer) };

			bool is_inside{ PointerIsInside(screen_pointer, world_pointer, entity) };

			if ((!interactive.was_inside && is_inside) || (interactive.was_inside && !is_inside)) {
				send_mouse_event = true;
			}
			if (top_only_) {
				if (is_inside) {
					auto depth{ entity.GetDepth() };
					if (depth >= top_depth || !top_entity) {
						top_depth  = depth;
						top_entity = entity;
					}
				} else {
					interactive.is_inside = false;
				}
			} else {
				interactive.is_inside = is_inside;
			}
		}
	}
	if (top_only_ && top_entity) {
		PTGN_ASSERT(top_entity.Has<Enabled>() && top_entity.IsEnabled());
		PTGN_ASSERT(top_entity.Has<Interactive>());
		auto& top_interactive{ top_entity.Get<Interactive>() };
		top_interactive.is_inside = true;
	}
	if (send_mouse_event) {
		OnMouseEvent(MouseEvent::Move, MouseMoveEvent{});
	}
}

void SceneInput::EntityMouseMove(
	Entity entity, const Interactive& interactive, const V2_float& screen_pointer
) const {
	auto world_pointer{ entity.GetCamera().TransformToCamera(screen_pointer) };

	entity.InvokeScript<&impl::IScript::OnMouseMove>(world_pointer);
	bool entered{ interactive.is_inside && !interactive.was_inside };
	bool exited{ !interactive.is_inside && interactive.was_inside };
	if (entered) {
		entity.InvokeScript<&impl::IScript::OnMouseEnter>(world_pointer);
	}
	if (exited) {
		entity.InvokeScript<&impl::IScript::OnMouseLeave>(world_pointer);
	}
	if (interactive.is_inside) {
		entity.InvokeScript<&impl::IScript::OnMouseOver>(world_pointer);
	} else {
		entity.InvokeScript<&impl::IScript::OnMouseOut>(world_pointer);
	}
	if (entity.Has<Draggable>() && entity.Get<Draggable>().dragging) {
		entity.InvokeScript<&impl::IScript::OnDrag>(world_pointer);
		if (interactive.is_inside) {
			entity.InvokeScript<&impl::IScript::OnDragOver>(world_pointer);
			if (!interactive.was_inside) {
				entity.InvokeScript<&impl::IScript::OnDragEnter>(world_pointer);
			}
		} else {
			entity.InvokeScript<&impl::IScript::OnDragOut>(world_pointer);
			if (interactive.was_inside) {
				entity.InvokeScript<&impl::IScript::OnDragLeave>(world_pointer);
			}
		}
	}
}

std::vector<Entity> GetOverlappingDropzones(
	Scene& scene, const Entity& entity, const V2_float& world_pointer
) {
	std::vector<Entity> overlapping_dropzones;
	for (auto [dropzone_entity, dropzone_enabled, dropzone_interactive, dropzone] :
		 scene.EntitiesWith<Enabled, Interactive, Dropzone>()) {
		bool is_inside{ false };
		switch (dropzone.trigger) {
			case DropTrigger::MouseOverlaps: {
				is_inside = Overlap(world_pointer, entity);
				break;
			}
			case DropTrigger::CenterOverlaps: {
				PTGN_ASSERT(
					entity.GetCamera() == dropzone_entity.GetCamera(),
					"Dropzone entity and drag entity must share the same camera"
				);
				is_inside = Overlap(entity.GetAbsolutePosition(), dropzone_entity);
				break;
			}
			case DropTrigger::Overlaps: {
				PTGN_ASSERT(
					entity.GetCamera() == dropzone_entity.GetCamera(),
					"Dropzone entity and drag entity must share the same camera"
				);
				is_inside = Overlap(entity, dropzone_entity);
				break;
			}
			case DropTrigger::Contains:
				// TODO: Implement.
				PTGN_ERROR("Unimplemented drop trigger");
				break;
			default: PTGN_ERROR("Unrecognized drop trigger");
		}
		if (is_inside) {
			overlapping_dropzones.emplace_back(dropzone_entity);
		}
	}
	return overlapping_dropzones;
}

void SceneInput::OnMouseEvent(MouseEvent type, const Event& event) {
	// TODO: Figure out a smart way to cache the scene.
	auto& scene{ game.scene.Get<Scene>(scene_key_) };
	auto screen_pointer{ game.input.GetMousePositionUnclamped() };
	switch (type) {
		case MouseEvent::Move: {
			// Prevent mouse move event from triggering twice per frame.
			if (triggered_callbacks_) {
				return;
			} else {
				triggered_callbacks_ = true;
			}
			for (auto [entity, enabled, interactive] : scene.EntitiesWith<Enabled, Interactive>()) {
				if (!enabled) {
					continue;
				}
				EntityMouseMove(entity, interactive, screen_pointer);
			}
			break;
		}
		case MouseEvent::Down: {
			Mouse mouse{ static_cast<const MouseDownEvent&>(event).mouse };
			for (auto [entity, enabled, interactive] : scene.EntitiesWith<Enabled, Interactive>()) {
				if (!enabled) {
					continue;
				}
				if (interactive.is_inside) {
					entity.InvokeScript<&impl::IScript::OnMouseDown>(mouse);
					if (entity.Has<Draggable>()) {
						if (auto& draggable{ entity.Get<Draggable>() }; !draggable.dragging) {
							auto world_pointer{ entity.GetCamera().TransformToCamera(screen_pointer
							) };
							draggable.dragging = true;
							draggable.start	   = world_pointer;
							draggable.offset   = entity.GetPosition() - draggable.start;
							entity.InvokeScript<&impl::IScript::OnDragStart>(world_pointer);
							auto overlapping_dropzones =
								GetOverlappingDropzones(scene, entity, world_pointer);
							Depth top_depth;
							Entity top_entity;
							for (const auto& dropzone : overlapping_dropzones) {
								if (!top_only_) {
									dropzone.Get<Dropzone>().entities.erase(entity);
									entity.InvokeScript<&impl::IScript::OnPickup>(dropzone);
								} else {
									auto depth{ dropzone.GetDepth() };
									if (depth >= top_depth || !top_entity) {
										top_depth  = depth;
										top_entity = dropzone;
									}
								}
							}
							if (top_only_ && top_entity) {
								top_entity.Get<Dropzone>().entities.erase(entity);
								entity.InvokeScript<&impl::IScript::OnPickup>(top_entity);
							}
						}
					}
				} else {
					entity.InvokeScript<&impl::IScript::OnMouseDownOutside>(mouse);
				}
			}
			break;
		}
		case MouseEvent::Up: {
			Mouse mouse{ static_cast<const MouseUpEvent&>(event).mouse };
			for (auto [entity, enabled, interactive] : scene.EntitiesWith<Enabled, Interactive>()) {
				if (!enabled) {
					continue;
				}
				if (interactive.is_inside) {
					entity.InvokeScript<&impl::IScript::OnMouseUp>(mouse);
				} else {
					entity.InvokeScript<&impl::IScript::OnMouseUpOutside>(mouse);
				}
				if (entity.Has<Draggable>()) {
					if (auto& draggable{ entity.Get<Draggable>() }; draggable.dragging) {
						auto world_pointer{ entity.GetCamera().TransformToCamera(screen_pointer) };
						draggable.dragging = false;
						draggable.offset   = {};
						entity.InvokeScript<&impl::IScript::OnDragStop>(world_pointer);
						auto overlapping_dropzones =
							GetOverlappingDropzones(scene, entity, world_pointer);
						Entity top_entity;
						Depth top_depth;
						for (const auto& dropzone : overlapping_dropzones) {
							if (!top_only_) {
								entity.InvokeScript<&impl::IScript::OnDrop>(dropzone);
								dropzone.Get<Dropzone>().entities.emplace(entity);
							} else {
								auto depth{ dropzone.GetDepth() };
								if (depth >= top_depth || !top_entity) {
									top_depth  = depth;
									top_entity = dropzone;
								}
							}
						}
						if (top_only_ && top_entity) {
							entity.InvokeScript<&impl::IScript::OnDrop>(top_entity);
							top_entity.Get<Dropzone>().entities.emplace(entity);
						}
					}
				}
			}
			break;
		}
		case MouseEvent::Pressed: {
			Mouse mouse{ static_cast<const MousePressedEvent&>(event).mouse };
			for (const auto& [entity, enabled, interactive, scripts] :
				 scene.EntitiesWith<Enabled, Interactive, Scripts>()) {
				if (!enabled) {
					continue;
				}
				if (interactive.is_inside) {
					entity.InvokeScript<&impl::IScript::OnMousePressed>(mouse);
				}
			}
			break;
		}
		case MouseEvent::Scroll: {
			V2_int scroll{ static_cast<const MouseScrollEvent&>(event).scroll };
			for (auto [entity, enabled, interactive, scripts] :
				 scene.EntitiesWith<Enabled, Interactive, Scripts>()) {
				if (!enabled) {
					continue;
				}
				if (interactive.is_inside) {
					entity.InvokeScript<&impl::IScript::OnMouseScroll>(scroll);
				}
			}
			break;
		}
		default: PTGN_ERROR("Unimplemented mouse event type");
	}
}

void SceneInput::OnKeyEvent(KeyEvent type, const Event& event) {
	auto& scene{ game.scene.Get<Scene>(scene_key_) };
	switch (type) {
		case KeyEvent::Down: {
			Key key{ static_cast<const KeyDownEvent&>(event).key };
			for (auto [entity, enabled, interactive, scripts] :
				 scene.EntitiesWith<Enabled, Interactive, Scripts>()) {
				if (!enabled) {
					continue;
				}
				entity.InvokeScript<&impl::IScript::OnKeyDown>(key);
			}
			break;
		}
		case KeyEvent::Up: {
			Key key{ static_cast<const KeyUpEvent&>(event).key };
			for (auto [entity, enabled, interactive, scripts] :
				 scene.EntitiesWith<Enabled, Interactive, Scripts>()) {
				if (!enabled) {
					continue;
				}
				entity.InvokeScript<&impl::IScript::OnKeyUp>(key);
			}
			break;
		}
		case KeyEvent::Pressed: {
			Key key{ static_cast<const KeyPressedEvent&>(event).key };
			for (auto [entity, enabled, interactive, scripts] :
				 scene.EntitiesWith<Enabled, Interactive, Scripts>()) {
				if (!enabled) {
					continue;
				}
				entity.InvokeScript<&impl::IScript::OnKeyPressed>(key);
			}
			break;
		}
		default: PTGN_ERROR("Unimplemented key event type");
	}
}

void SceneInput::ResetInteractives(Scene& scene) {
	for (auto [entity, enabled, interactive] : scene.EntitiesWith<Enabled, Interactive>()) {
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
	game.input.ResetKeyStates();
	game.input.ResetMouseStates();
	game.input.Update();

	auto& scene{ game.scene.Get<Scene>(scene_key_) };

	ResetInteractives(scene);
	UpdateCurrent(scene);
	OnMouseEvent(MouseEvent::Move, MouseMoveEvent{});

	// TODO: Cache interactive entity list every frame to avoid repeated calls for each
	// mouse and keyboard event type.

	game.event.key.Subscribe(
		this, std::bind(&SceneInput::OnKeyEvent, this, std::placeholders::_1, std::placeholders::_2)
	);

	game.event.mouse.Subscribe(
		this,
		std::bind(&SceneInput::OnMouseEvent, this, std::placeholders::_1, std::placeholders::_2)
	);
}

void SceneInput::Shutdown() {
	game.event.key.Unsubscribe(this);
	game.event.mouse.Unsubscribe(this);
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

void SimulateMouseMovement(Entity entity) {
	PTGN_ASSERT(entity.Has<impl::SceneKey>());
	const auto& scene_key{ entity.Get<impl::SceneKey>() };
	const auto& scene{ game.scene.Get<Scene>(scene_key) };
	PTGN_ASSERT(entity.Has<Interactive>());
	auto& interactive{ entity.Get<Interactive>() };
	interactive.was_inside = false;
	auto screen_pointer{ game.input.GetMousePositionUnclamped() };
	scene.input.EntityMouseMove(entity, interactive, screen_pointer);
}

} // namespace ptgn