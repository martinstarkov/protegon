#include "scene/scene_input.h"

#include <functional>

#include "common/assert.h"
#include "common/function.h"
#include "components/common.h"
#include "components/draw.h"
#include "components/input.h"
#include "core/entity.h"
#include "core/game.h"
#include "debug/log.h"
#include "events/event.h"
#include "events/event_handler.h"
#include "events/events.h"
#include "events/input_handler.h"
#include "events/key.h"
#include "events/mouse.h"
#include "math/math.h"
#include "math/vector2.h"
#include "physics/collision/overlap.h"
#include "rendering/api/origin.h"
#include "rendering/renderer.h"
#include "rendering/resources/texture.h"
#include "rendering/resources/render_target.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "ui/button.h"

namespace ptgn {

bool SceneInput::PointerIsInside(const V2_float& pointer, const Entity& entity) {
	bool is_circle{ entity.Has<InteractiveCircles>() };
	bool is_rect{ entity.Has<InteractiveRects>() };
	PTGN_ASSERT(
		!(is_rect && is_circle),
		"Entity cannot have both an interactive radius and an interactive size"
	);
	auto scale{ entity.GetScale() };
	auto pos{ entity.GetAbsolutePosition() };
	auto origin{ entity.GetOrigin() };
	bool overlapping{ false };
	if (is_rect || is_circle) {
		std::size_t count{ 0 };
		if (is_rect) {
			auto rotation{ entity.GetRotation() };
			const auto& interactives{ entity.Get<InteractiveRects>() };
			count += interactives.rects.size();
			for (const auto& interactive : interactives.rects) {
				auto size{ interactive.size * Abs(scale) };
				origin = interactive.origin;
				auto center{ pos + interactive.offset * Abs(scale) +
							 GetOriginOffset(origin, size) };
				if (draw_interactives_) {
					DrawDebugRect(center, size, color::Magenta, Origin::Center, 1.0f, rotation);
				}
				if (impl::OverlapPointRect(pointer, center, size, rotation)) {
					if (draw_interactives_) {
						overlapping = true;
					} else {
						return true;
					}
				}
			}
		}
		if (is_circle) {
			const auto& interactives{ entity.Get<InteractiveCircles>() };
			count += interactives.circles.size();
			for (const auto& interactive : interactives.circles) {
				auto radius{ interactive.radius * Abs(scale.x) };
				auto center{ pos + interactive.offset * Abs(scale) };
				if (draw_interactives_) {
					DrawDebugCircle(center, radius, color::Magenta, 1.0f);
				}
				if (impl::OverlapPointCircle(pointer, center, radius)) {
					if (draw_interactives_) {
						overlapping = true;
					} else {
						return true;
					}
				}
			}
		}
		if (count) {
			if (draw_interactives_) {
				if (overlapping) {
					return true;
				}
			}
			return false;
		}
	}

	// TODO: Consider readding this with circle and rect objects.
	is_circle = entity.Has<impl::ButtonRadius>();
	is_rect	  = entity.Has<impl::ButtonSize>();
	if (is_circle || is_rect) {
		bool zero_sized{ false };
		if (is_circle) {
			float radius{ entity.Get<impl::ButtonRadius>() };
			if (radius == 0.0f) {
				zero_sized = true;
			} else {
				auto radius_scaled{ radius * Abs(scale.x) };
				zero_sized = false;
				if (draw_interactives_) {
					DrawDebugCircle(pos, radius_scaled, color::Magenta, 1.0f);
				}
				if (impl ::OverlapPointCircle(pointer, pos, radius_scaled)) {
					return true;
				}
			}
		}
		if (is_rect) {
			V2_float size{ entity.Get<impl::ButtonSize>() };
			if (size.IsZero()) {
				zero_sized = true;
			} else {
				zero_sized = false;
				auto size_scaled{ size * Abs(scale) };
				origin = entity.GetOrigin();
				auto center{ pos + GetOriginOffset(origin, size_scaled) };
				auto rotation{ entity.GetRotation() };
				if (draw_interactives_) {
					DrawDebugRect(
						center, size_scaled, color::Magenta, Origin::Center, 1.0f, rotation
					);
				}
				if (impl::OverlapPointRect(pointer, center, size_scaled, rotation)) {
					return true;
				}
			}
		}
		if (!zero_sized) {
			return false;
		}
	}

	if (entity.Has<TextureHandle>()) {
		const auto& texture_key{ entity.Get<TextureHandle>() };
		auto size_scaled{ texture_key.GetSize(entity) * Abs(scale) };
		auto center{ pos + GetOriginOffset(origin, size_scaled) };
		auto rotation{ entity.GetRotation() };
		if (draw_interactives_) {
			DrawDebugRect(center, size_scaled, color::Magenta, Origin::Center, 1.0f, rotation);
		}
		return impl::OverlapPointRect(pointer, center, size_scaled, rotation);
	}

	return false;
}

void SceneInput::UpdatePrevious(Scene* scene) {
	PTGN_ASSERT(scene != nullptr);
	for (auto [e, enabled, interactive] : scene->manager.EntitiesWith<Enabled, Interactive>()) {
		if (!enabled) {
			continue;
		}
		interactive.was_inside = interactive.is_inside;
		interactive.is_inside  = false;
	}
}

void SceneInput::UpdateCurrent(Scene* scene) {
	PTGN_ASSERT(scene != nullptr);
	V2_float mouse_pos{ GetMousePosition() };
	auto pos{ mouse_pos };
	Depth top_depth;
	Entity top_entity;
	bool send_mouse_event{ false };
	for (auto [entity, enabled, interactive] : scene->manager.EntitiesWith<Enabled, Interactive>()) {
		if (!enabled) {
			interactive.is_inside  = false;
			interactive.was_inside = false;
		} else {
			if (entity.Has<Camera>()) {
				auto screen_pos{ game.input.GetMousePosition() };
				const auto& camera{ entity.Get<Camera>() };
				pos = camera.TransformToCamera(screen_pos);
			} else if (entity.Has<RenderTarget>()) {
				auto screen_pos{ game.input.GetMousePosition() };
				const auto& camera{ entity.Get<RenderTarget>().GetCamera() };
				pos = camera.TransformToCamera(screen_pos);
			} else {
				pos = mouse_pos;
			}
			bool is_inside{ PointerIsInside(pos, entity) };
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
		PTGN_ASSERT(top_entity.Has<Enabled>() && top_entity.Get<Enabled>());
		PTGN_ASSERT(top_entity.Has<Interactive>());
		auto& top_interactive{ top_entity.Get<Interactive>() };
		top_interactive.is_inside = true;
	}
	if (send_mouse_event) {
		OnMouseEvent(MouseEvent::Move, MouseMoveEvent{});
	}
}

void SceneInput::OnMouseEvent(MouseEvent type, const Event& event) {
	// TODO: Figure out a smart way to cache the scene.
	auto& scene{ game.scene.Get<Scene>(scene_key_) };
	// TODO: Use entity's camera.
	V2_float pos{ GetMousePosition() };
	switch (type) {
		case MouseEvent::Move: {
			for (auto [e, enabled, interactive] :
				 scene.manager.EntitiesWith<Enabled, Interactive>()) {
				if (!enabled) {
					continue;
				}
				Invoke<callback::MouseMove>(e, pos);
				bool entered{ interactive.is_inside && !interactive.was_inside };
				bool exited{ !interactive.is_inside && interactive.was_inside };
				if (entered) {
					Invoke<callback::MouseEnter>(e, pos);
				}
				if (exited) {
					Invoke<callback::MouseLeave>(e, pos);
				}
				if (interactive.is_inside) {
					Invoke<callback::MouseOver>(e, pos);
				} else {
					Invoke<callback::MouseOut>(e, pos);
				}
				if (e.Has<Draggable>() && e.Get<Draggable>().dragging) {
					Invoke<callback::Drag>(e, pos);
					if (interactive.is_inside) {
						Invoke<callback::DragOver>(e, pos);
						if (!interactive.was_inside) {
							Invoke<callback::DragEnter>(e, pos);
						}
					} else {
						Invoke<callback::DragOut>(e, pos);
						if (interactive.was_inside) {
							Invoke<callback::DragLeave>(e, pos);
						}
					}
				}
			}
			break;
		}
		case MouseEvent::Down: {
			Mouse mouse{ static_cast<const MouseDownEvent&>(event).mouse };
			for (auto [e, enabled, interactive] :
				 scene.manager.EntitiesWith<Enabled, Interactive>()) {
				if (!enabled) {
					continue;
				}
				if (interactive.is_inside) {
					Invoke<callback::MouseDown>(e, mouse);
					if (e.Has<Draggable>()) {
						if (auto& draggable{ e.Get<Draggable>() }; !draggable.dragging) {
							draggable.dragging = true;
							draggable.start	   = pos;
							draggable.offset   = e.GetPosition() - draggable.start;
							Invoke<callback::DragStart>(e, pos);
						}
					}
				} else {
					Invoke<callback::MouseDownOutside>(e, mouse);
				}
			}
			break;
		}
		case MouseEvent::Up: {
			Mouse mouse{ static_cast<const MouseUpEvent&>(event).mouse };
			for (auto [e, enabled, interactive] :
				 scene.manager.EntitiesWith<Enabled, Interactive>()) {
				if (!enabled) {
					continue;
				}
				if (interactive.is_inside) {
					Invoke<callback::MouseUp>(e, mouse);
				} else {
					Invoke<callback::MouseUpOutside>(e, mouse);
				}
				if (e.Has<Draggable>()) {
					if (auto& draggable{ e.Get<Draggable>() }; draggable.dragging) {
						draggable.dragging = false;
						draggable.offset   = {};
						Invoke<callback::DragStop>(e, pos);
					}
				}
			}
			break;
		}
		case MouseEvent::Pressed: {
			Mouse mouse{ static_cast<const MousePressedEvent&>(event).mouse };
			for (const auto& [e, enabled, interactive, callback] :
				 scene.manager.EntitiesWith<Enabled, Interactive, callback::MousePressed>()) {
				if (!enabled) {
					continue;
				}
				if (interactive.is_inside) {
					callback(mouse);
				}
			}
			break;
		}
		case MouseEvent::Scroll: {
			V2_int scroll{ static_cast<const MouseScrollEvent&>(event).scroll };
			for (auto [e, enabled, interactive, callback] :
				 scene.manager.EntitiesWith<Enabled, Interactive, callback::MouseScroll>()) {
				if (!enabled) {
					continue;
				}
				if (interactive.is_inside) {
					Invoke(callback, scroll);
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
			for (auto [e, enabled, interactive, callback] :
				 scene.manager.EntitiesWith<Enabled, Interactive, callback::KeyDown>()) {
				if (!enabled) {
					continue;
				}
				Invoke(callback, key);
			}
			break;
		}
		case KeyEvent::Up: {
			Key key{ static_cast<const KeyUpEvent&>(event).key };
			for (auto [e, enabled, interactive, callback] :
				 scene.manager.EntitiesWith<Enabled, Interactive, callback::KeyUp>()) {
				if (!enabled) {
					continue;
				}
				Invoke(callback, key);
			}
			break;
		}
		case KeyEvent::Pressed: {
			Key key{ static_cast<const KeyPressedEvent&>(event).key };
			for (auto [e, enabled, interactive, callback] :
				 scene.manager.EntitiesWith<Enabled, Interactive, callback::KeyPressed>()) {
				if (!enabled) {
					continue;
				}
				Invoke(callback, key);
			}
			break;
		}
		default: PTGN_ERROR("Unimplemented key event type");
	}
}

void SceneInput::ResetInteractives(Scene* scene) {
	for (auto [e, enabled, interactive] : scene->manager.EntitiesWith<Enabled, Interactive>()) {
		interactive.was_inside = false;
		interactive.is_inside  = false;
	}
}

void SceneInput::Init(std::size_t scene_key) {
	if (draw_interactives_) {
		PTGN_WARN("Drawing interactable hitboxes");
	}

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