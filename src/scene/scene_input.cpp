#include "scene/scene_input.h"

#include <functional>
#include <optional>
#include <variant>
#include <vector>

#include "common/assert.h"
#include "components/common.h"
#include "components/input.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/window.h"
#include "debug/log.h"
#include "events/event.h"
#include "events/event_handler.h"
#include "events/events.h"
#include "events/input_handler.h"
#include "events/key.h"
#include "events/mouse.h"
#include "math/vector2.h"
#include "physics/collision/collision_handler.h"
#include "physics/collision/overlap.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/graphics/circle.h"
#include "rendering/graphics/rect.h"
#include "rendering/renderer.h"
#include "rendering/resources/render_target.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_key.h"
#include "scene/scene_manager.h"

// TODO: Clean up this mess of a file.

namespace ptgn {

struct BoxInfo {
	V2_float center;
	V2_float size;
	float rotation{ 0.0f };
	std::optional<V2_float> rotation_center{ std::nullopt };
};

struct CircleInfo {
	V2_float center;
	float radius{ 0.0f };
};

std::variant<std::vector<BoxInfo>, std::vector<CircleInfo>> GetInteractives(
	const Entity& entity, const Camera& camera
) {
	bool is_circle{ entity.Has<InteractiveCircles>() };
	bool is_rect{ entity.Has<InteractiveRects>() };
	PTGN_ASSERT(
		!(is_rect && is_circle),
		"Entity cannot have both an interactive radius and an interactive size"
	);
	auto scale{ Abs(entity.GetScale()) };
	auto pos{ entity.GetAbsolutePosition() };
	auto origin{ entity.GetOrigin() };

	std::variant<std::vector<BoxInfo>, std::vector<CircleInfo>> info;

	if (is_rect || is_circle) {
		if (is_rect) {
			auto rotation{ entity.GetRotation() };
			auto rotation_center{ entity.GetRotationCenter() };
			const auto& interactives{ entity.Get<InteractiveRects>() };

			info.emplace<std::vector<BoxInfo>>();

			for (const auto& interactive : interactives.rects) {
				auto size{ interactive.size * scale };
				origin = interactive.origin;
				auto center{ pos + interactive.offset * scale + GetOriginOffset(origin, size) };
				std::get<std::vector<BoxInfo>>(info).emplace_back(BoxInfo{
					camera.TransformToScreen(center), camera.ScaleToScreen(size), rotation,
					rotation_center });
			}
		}
		if (is_circle) {
			const auto& interactives{ entity.Get<InteractiveCircles>() };
			info.emplace<std::vector<CircleInfo>>();
			for (const auto& interactive : interactives.circles) {
				auto radius{ interactive.radius * scale.x };
				auto center{ pos + interactive.offset * scale };
				std::get<std::vector<CircleInfo>>(info).emplace_back(CircleInfo{
					camera.TransformToScreen(center), camera.ScaleToScreen(radius) });
			}
		}
		if (!std::get<std::vector<CircleInfo>>(info).empty()) {
			return info;
		}
	}

	is_circle = entity.Has<Circle>();
	is_rect	  = entity.Has<Rect>();
	if (is_circle || is_rect) {
		if (is_circle) {
			float radius{ entity.Get<Circle>().radius };
			if (radius != 0.0f) {
				auto radius_scaled{ radius * scale.x };
				info.emplace<std::vector<CircleInfo>>();
				std::get<std::vector<CircleInfo>>(info).emplace_back(CircleInfo{
					camera.TransformToScreen(pos), camera.ScaleToScreen(radius_scaled) });
				return info;
			}
		}
		if (is_rect) {
			V2_float size{ entity.Get<Rect>().size };
			if (!size.IsZero()) {
				auto size_scaled{ size * scale };
				origin = entity.GetOrigin();
				auto center{ pos + GetOriginOffset(origin, size_scaled) };
				auto rotation{ entity.GetRotation() };
				auto rotation_center{ entity.GetRotationCenter() };
				info.emplace<std::vector<BoxInfo>>();
				std::get<std::vector<BoxInfo>>(info).emplace_back(BoxInfo{
					camera.TransformToScreen(center), camera.ScaleToScreen(size_scaled), rotation,
					rotation_center });
				return info;
			}
		}
	}

	info.emplace<std::vector<BoxInfo>>();

	if (entity.Has<TextureHandle>()) {
		const auto& texture_key{ entity.Get<TextureHandle>() };
		auto size_scaled{ texture_key.GetSize(entity) * scale };
		auto center{ pos + GetOriginOffset(origin, size_scaled) };
		auto rotation{ entity.GetRotation() };
		auto rotation_center{ entity.GetRotationCenter() };
		std::get<std::vector<BoxInfo>>(info).emplace_back(BoxInfo{
			camera.TransformToScreen(center), camera.ScaleToScreen(size_scaled), rotation,
			rotation_center });
	}

	PTGN_ASSERT(!std::get<std::vector<BoxInfo>>(info).empty());

	return info;
}

bool InteractivesOverlap(const Entity& a, const Entity& b, const Camera& camera) {
	auto interactivesA = GetInteractives(a, camera);
	auto interactivesB = GetInteractives(b, camera);

	if (std::holds_alternative<std::vector<BoxInfo>>(interactivesA)) {
		const auto& boxesA = std::get<std::vector<BoxInfo>>(interactivesA);
		if (std::holds_alternative<std::vector<BoxInfo>>(interactivesB)) {
			const auto& boxesB = std::get<std::vector<BoxInfo>>(interactivesB);
			for (const auto& boxA : boxesA) {
				for (const auto& boxB : boxesB) {
					if (impl::OverlapRectRect(
							boxA.center, boxA.size, boxA.rotation, boxA.rotation_center,
							boxB.center, boxB.size, boxB.rotation, boxB.rotation_center
						)) {
						return true;
					}
				}
			}
		} else if (std::holds_alternative<std::vector<CircleInfo>>(interactivesB)) {
			const auto& circlesB = std::get<std::vector<CircleInfo>>(interactivesB);
			for (const auto& boxA : boxesA) {
				for (const auto& circleB : circlesB) {
					if (impl::OverlapCircleRect(
							circleB.center, circleB.radius, boxA.center, boxA.size, boxA.rotation,
							boxA.rotation_center
						)) {
						return true;
					}
				}
			}
		}
	} else if (std::holds_alternative<std::vector<CircleInfo>>(interactivesA)) {
		const auto& circlesA = std::get<std::vector<CircleInfo>>(interactivesA);
		if (std::holds_alternative<std::vector<BoxInfo>>(interactivesB)) {
			const auto& boxesB = std::get<std::vector<BoxInfo>>(interactivesB);
			for (const auto& circleA : circlesA) {
				for (const auto& boxB : boxesB) {
					if (impl::OverlapCircleRect(
							circleA.center, circleA.radius, boxB.center, boxB.size, boxB.rotation,
							boxB.rotation_center
						)) {
						return true;
					}
				}
			}
		} else if (std::holds_alternative<std::vector<CircleInfo>>(interactivesB)) {
			const auto& circlesB = std::get<std::vector<CircleInfo>>(interactivesB);
			for (const auto& circleA : circlesA) {
				for (const auto& circleB : circlesB) {
					if (impl::OverlapCircleCircle(
							circleA.center, circleA.radius, circleB.center, circleB.radius
						)) {
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool PointOverlapsInteractives(V2_float point, const Entity& entity, const Camera& camera) {
	PTGN_ASSERT(camera);
	auto interactives = GetInteractives(entity, camera);

	if (std::holds_alternative<std::vector<BoxInfo>>(interactives)) {
		const auto& boxes = std::get<std::vector<BoxInfo>>(interactives);
		for (const auto& box : boxes) {
			DrawDebugRect(
				box.center, box.size, color::Magenta, Origin::Center, 1.0f, box.rotation, camera
			);
			if (impl::OverlapPointRect(
					point, box.center, box.size, box.rotation, box.rotation_center
				)) {
				return true;
			}
		}
	} else if (std::holds_alternative<std::vector<CircleInfo>>(interactives)) {
		const auto& circles = std::get<std::vector<CircleInfo>>(interactives);
		for (const auto& circle : circles) {
			DrawDebugCircle(circle.center, circle.radius, color::Magenta, 1.0f, camera);
			if (impl::OverlapPointCircle(point, circle.center, circle.radius)) {
				return true;
			}
		}
	}

	return false;
}

bool SceneInput::PointerIsInside(V2_float pointer, const Camera& camera, const Entity& entity)
	const {
	pointer = game.input.GetMousePositionUnclamped();
	auto window_size{ game.window.GetSize() };
	if (!impl::OverlapPointRect(pointer, window_size / 2.0f, window_size, 0.0f, std::nullopt)) {
		// Mouse outside of screen.
		return false;
	}

	return PointOverlapsInteractives(pointer, entity, camera);
}

void SceneInput::UpdatePrevious(Scene* scene) {
	triggered_callbacks_ = false;
	PTGN_ASSERT(scene != nullptr);
	for (auto [entity, enabled, interactive] : scene->EntitiesWith<Enabled, Interactive>()) {
		if (!enabled) {
			continue;
		}
		interactive.was_inside = interactive.is_inside;
		interactive.is_inside  = false;
	}
}

RenderTarget GetParentRenderTarget(const Entity& entity) {
	if (entity.Has<RenderTarget>()) {
		return entity.Get<RenderTarget>();
	}
	if (entity.HasParent()) {
		return GetParentRenderTarget(entity.GetParent());
	}
	return entity;
}

void GetMousePosAndCamera(
	const Entity& entity, const V2_float& mouse_pos, const Camera& primary, V2_float& pos,
	Camera& camera
) {
	// TODO: Figure out if there is a better way to design this, without having to
	// recursively search an entity family hierarchy. The issue lies in the fact that when
	// something such as a button is rendered to a render target, its interaction coordinate
	// system departs from its drawing coordinate system, which leads to unexpected scaling
	// and offsets.
	if (entity.Has<Camera>()) {
		camera = entity.Get<Camera>();
	} else if (entity.Has<RenderTarget>()) {
		if (auto& rt{ entity.Get<RenderTarget>() }; rt.Has<Camera>()) {
			camera = rt.Get<Camera>();
		}
	} else if (auto rt{ GetParentRenderTarget(entity) }; rt != entity && rt && rt.Has<Camera>()) {
		camera = rt.Get<Camera>();
	} else {
		camera = primary;
		pos	   = mouse_pos;
		return;
	}
	if (!camera) {
		camera = primary;
	}
	pos = camera.TransformToCamera(mouse_pos);
}

void SceneInput::UpdateCurrent(Scene* scene) {
	PTGN_ASSERT(scene != nullptr);
	// auto mouse_pos{ game.input.GetMousePosition() };
	auto mouse_pos{ game.input.GetMousePositionUnclamped() };
	auto pos{ mouse_pos };
	Camera camera;
	if (scene->camera.primary && scene->camera.primary.Has<impl::CameraInfo>()) {
		pos	   = scene->camera.primary.TransformToCamera(mouse_pos);
		camera = scene->camera.primary;
	} else {
		// Scene camera has not been set yet.
		return;
	}
	Depth top_depth;
	Entity top_entity;
	bool send_mouse_event{ false };
	for (auto [entity, enabled, interactive] : scene->EntitiesWith<Enabled, Interactive>()) {
		if (!enabled) {
			interactive.is_inside  = false;
			interactive.was_inside = false;
		} else {
			GetMousePosAndCamera(entity, mouse_pos, scene->camera.primary, pos, camera);
			bool is_inside{ PointerIsInside(pos, camera, entity) };
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
	const Scene& scene, Entity entity, const Interactive& interactive, const V2_float& mouse_pos,
	V2_float& pos, Camera& camera
) const {
	GetMousePosAndCamera(entity, mouse_pos, scene.camera.primary, pos, camera);
	entity.InvokeScript<&impl::IScript::OnMouseMove>(pos);
	bool entered{ interactive.is_inside && !interactive.was_inside };
	bool exited{ !interactive.is_inside && interactive.was_inside };
	if (entered) {
		entity.InvokeScript<&impl::IScript::OnMouseEnter>(pos);
	}
	if (exited) {
		entity.InvokeScript<&impl::IScript::OnMouseLeave>(pos);
	}
	if (interactive.is_inside) {
		entity.InvokeScript<&impl::IScript::OnMouseOver>(pos);
	} else {
		entity.InvokeScript<&impl::IScript::OnMouseOut>(pos);
	}
	if (entity.Has<Draggable>() && entity.Get<Draggable>().dragging) {
		entity.InvokeScript<&impl::IScript::OnDrag>(pos);
		if (interactive.is_inside) {
			entity.InvokeScript<&impl::IScript::OnDragOver>(pos);
			if (!interactive.was_inside) {
				entity.InvokeScript<&impl::IScript::OnDragEnter>(pos);
			}
		} else {
			entity.InvokeScript<&impl::IScript::OnDragOut>(pos);
			if (interactive.was_inside) {
				entity.InvokeScript<&impl::IScript::OnDragLeave>(pos);
			}
		}
	}
}

std::vector<Entity> GetOverlappingDropzones(
	Scene& scene, const Entity& entity, const V2_float& mouse_position, const Camera& camera
) {
	std::vector<Entity> overlapping_dropzones;
	for (auto [dropzone_entity, dropzone_enabled, dropzone_interactive, dropzone] :
		 scene.EntitiesWith<Enabled, Interactive, Dropzone>()) {
		bool is_inside{ false };
		switch (dropzone.trigger) {
			case DropTrigger::MouseOverlaps: {
				is_inside = PointOverlapsInteractives(mouse_position, dropzone_entity, camera);
				break;
			}
			case DropTrigger::CenterOverlaps: {
				is_inside = PointOverlapsInteractives(
					entity.GetAbsolutePosition(), dropzone_entity, camera
				);
				break;
			}
			case DropTrigger::Overlaps: {
				is_inside = InteractivesOverlap(entity, dropzone_entity, camera);
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
	auto mouse_pos{ game.input.GetMousePositionUnclamped() };
	auto pos{ mouse_pos };
	Camera camera; // unused
	if (scene.camera.primary && scene.camera.primary.Has<impl::CameraInfo>()) {
		pos = scene.camera.primary.TransformToCamera(mouse_pos);
	} else {
		// Scene camera has not been set yet.
		return;
	}
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
				EntityMouseMove(scene, entity, interactive, mouse_pos, pos, camera);
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
							GetMousePosAndCamera(
								entity, mouse_pos, scene.camera.primary, pos, camera
							);
							draggable.dragging = true;
							draggable.start	   = pos;
							draggable.offset   = entity.GetPosition() - draggable.start;
							entity.InvokeScript<&impl::IScript::OnDragStart>(pos);
							PTGN_ASSERT(camera);
							auto overlapping_dropzones =
								GetOverlappingDropzones(scene, entity, pos, camera);
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
						GetMousePosAndCamera(entity, mouse_pos, scene.camera.primary, pos, camera);
						draggable.dragging = false;
						draggable.offset   = {};
						entity.InvokeScript<&impl::IScript::OnDragStop>(pos);
						auto overlapping_dropzones =
							GetOverlappingDropzones(scene, entity, pos, camera);
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

void SceneInput::ResetInteractives(Scene* scene) {
	PTGN_ASSERT(scene != nullptr);
	for (auto [entity, enabled, interactive] : scene->EntitiesWith<Enabled, Interactive>()) {
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

	ResetInteractives(&scene);
	UpdateCurrent(&scene);
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
	ResetInteractives(&scene);

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
	auto mouse_pos{ game.input.GetMousePositionUnclamped() };
	auto pos{ mouse_pos };
	Camera camera; // unused
	if (scene.camera.primary && scene.camera.primary.Has<impl::CameraInfo>()) {
		pos = scene.camera.primary.TransformToCamera(mouse_pos);
	} else {
		// Scene camera has not been set yet.
		return;
	}
	scene.input.EntityMouseMove(scene, entity, interactive, mouse_pos, pos, camera);
}

} // namespace ptgn