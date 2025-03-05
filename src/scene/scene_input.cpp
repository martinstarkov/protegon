#include "scene_input.h"

#include <functional>

#include "components/draw.h"
#include "components/input.h"
#include "components/transform.h"
#include "core/game.h"
#include "core/game_object.h"
#include "ecs/ecs.h"
#include "event/event.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "event/mouse.h"
#include "math/collision/overlap.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "utility/assert.h"
#include "utility/log.h"
#include "utility/utility.h"

namespace ptgn {

bool SceneInput::PointerIsInside(const V2_float& pointer, const ecs::Entity& entity) {
	constexpr bool draw_interactives{ true };

	bool is_circle{ entity.Has<InteractiveCircles>() };
	bool is_rect{ entity.Has<InteractiveRects>() };
	PTGN_ASSERT(
		!(is_rect && is_circle),
		"Entity cannot have both an interactive radius and an interactive size"
	);
	auto scale{ GetScale(entity) };
	auto pos{ GetPosition(entity) };
	auto origin{ GetOrigin(entity) };
	bool overlapping{ false };
	if (is_rect || is_circle) {
		std::size_t count{ 0 };
		if (is_rect) {
			auto rotation{ GetRotation(entity) };
			const auto& interactives{ entity.Get<InteractiveRects>() };
			count += interactives.rects.size();
			for (const auto& interactive : interactives.rects) {
				auto size{ interactive.rect.size * scale };
				origin = interactive.rect.origin;
				auto center{ pos + interactive.offset * scale + GetOriginOffset(origin, size) };
				if constexpr (draw_interactives) {
					DrawDebugRect(center, size, color::Magenta, Origin::Center, 1.0f, rotation);
				}
				if (impl::OverlapPointRect(pointer, center, size, rotation)) {
					if constexpr (draw_interactives) {
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
				auto radius{ interactive.circle.radius * scale.x };
				auto center{ pos + interactive.offset * scale };
				if constexpr (draw_interactives) {
					DrawDebugCircle(center, radius, color::Magenta, 1.0f);
				}
				if (impl::OverlapPointCircle(pointer, center, radius)) {
					if constexpr (draw_interactives) {
						overlapping = true;
					} else {
						return true;
					}
				}
			}
		}
		if (count) {
			if constexpr (draw_interactives) {
				if (overlapping) {
					return true;
				}
			}
			return false;
		}
	}
	is_circle = entity.Has<Circle>();
	is_rect	  = entity.Has<Rect>();
	if (is_circle || is_rect) {
		bool zero_sized{ false };
		if (is_circle) {
			const auto& c{ entity.Get<Circle>() };
			if (c.radius == 0.0f) {
				zero_sized = true;
			} else {
				auto r{ c.radius * scale.x };
				zero_sized = false;
				if constexpr (draw_interactives) {
					DrawDebugCircle(pos, r, color::Magenta, 1.0f);
				}
				if (impl ::OverlapPointCircle(pointer, pos, r)) {
					return true;
				}
			}
		}
		if (is_rect) {
			const auto& r{ entity.Get<Rect>() };
			if (r.size.IsZero()) {
				zero_sized = true;
			} else {
				zero_sized = false;
				auto size{ r.size * scale };
				origin = r.origin;
				auto center{ pos + GetOriginOffset(origin, r.size) };
				auto rotation{ GetRotation(entity) };
				if constexpr (draw_interactives) {
					DrawDebugRect(center, size, color::Magenta, Origin::Center, 1.0f, rotation);
				}
				if (impl::OverlapPointRect(pointer, center, size, rotation)) {
					return true;
				}
			}
		}
		if (!zero_sized) {
			return false;
		}
	}

	if (entity.Has<TextureKey>()) {
		const auto& texture_key{ entity.Get<TextureKey>() };
		auto size{ game.texture.GetSize(texture_key) * scale };
		auto center{ pos + GetOriginOffset(origin, size) };
		auto rotation{ GetRotation(entity) };
		if constexpr (draw_interactives) {
			DrawDebugRect(center, size, color::Magenta, Origin::Center, 1.0f, rotation);
		}
		return impl::OverlapPointRect(pointer, center, size, rotation);
	}

	return false;
}

void SceneInput::UpdatePrevious() {
	PTGN_ASSERT(scene_ != nullptr);
	for (auto [e, enabled, interactive] : scene_->manager.EntitiesWith<Enabled, Interactive>()) {
		if (!enabled) {
			continue;
		}
		interactive.was_inside = interactive.is_inside;
	}
}

void SceneInput::UpdateCurrent() {
	PTGN_ASSERT(scene_ != nullptr);
	V2_float pos{ GetMousePosition() };
	Depth top_depth;
	ecs::Entity top_entity;
	bool send_mouse_event{ false };
	for (auto [e, enabled, interactive] : scene_->manager.EntitiesWith<Enabled, Interactive>()) {
		if (!enabled) {
			interactive.is_inside  = false;
			interactive.was_inside = false;
		} else {
			bool is_inside{ PointerIsInside(pos, e) };
			if (!interactive.was_inside && is_inside || interactive.was_inside && !is_inside) {
				send_mouse_event = true;
			}
			if (top_only_) {
				if (is_inside) {
					auto depth{ GetDepth(e) };
					if (depth >= top_depth || top_entity == ecs::Entity{}) {
						top_depth  = depth;
						top_entity = e;
					}
				} else {
					interactive.is_inside = false;
				}
			} else {
				interactive.is_inside = is_inside;
			}
		}
	}
	if (top_only_ && top_entity != ecs::Entity{}) {
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
	PTGN_ASSERT(scene_ != nullptr);
	V2_float pos{ GetMousePosition() };
	switch (type) {
		case MouseEvent::Move: {
			for (auto [e, enabled, interactive] :
				 scene_->manager.EntitiesWith<Enabled, Interactive>()) {
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
				 scene_->manager.EntitiesWith<Enabled, Interactive>()) {
				if (!enabled) {
					continue;
				}
				if (interactive.is_inside) {
					Invoke<callback::MouseDown>(e, mouse);
					if (e.Has<Draggable>()) {
						if (auto& draggable{ e.Get<Draggable>() }; !draggable.dragging) {
							draggable.dragging = true;
							draggable.start	   = pos;
							draggable.offset   = GetPosition(e) - draggable.start;
							draggable.target   = e;
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
				 scene_->manager.EntitiesWith<Enabled, Interactive>()) {
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
						draggable.target   = ecs::null;
						Invoke<callback::DragStop>(e, pos);
					}
				}
			}
			break;
		}
		case MouseEvent::Pressed: {
			Mouse mouse{ static_cast<const MousePressedEvent&>(event).mouse };
			for (const auto& [e, enabled, interactive, callback] :
				 scene_->manager.EntitiesWith<Enabled, Interactive, callback::MousePressed>()) {
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
				 scene_->manager.EntitiesWith<Enabled, Interactive, callback::MouseScroll>()) {
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
	PTGN_ASSERT(scene_ != nullptr);
	switch (type) {
		case KeyEvent::Down: {
			Key key{ static_cast<const KeyDownEvent&>(event).key };
			for (auto [e, enabled, interactive, callback] :
				 scene_->manager.EntitiesWith<Enabled, Interactive, callback::KeyDown>()) {
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
				 scene_->manager.EntitiesWith<Enabled, Interactive, callback::KeyUp>()) {
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
				 scene_->manager.EntitiesWith<Enabled, Interactive, callback::KeyPressed>()) {
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

void SceneInput::ResetInteractives() {
	for (auto [e, enabled, interactive] : scene_->manager.EntitiesWith<Enabled, Interactive>()) {
		interactive.was_inside = false;
		interactive.is_inside  = false;
	}
}

void SceneInput::Init(Scene* scene) {
	scene_ = scene;
	// Input is reset to ensure no previously pressed keys are considered held.
	game.input.ResetKeyStates();
	game.input.ResetMouseStates();
	game.input.Update();

	ResetInteractives();
	UpdateCurrent();
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
	ResetInteractives();
}

void SceneInput::SetTopOnly(bool top_only) {
	top_only_ = top_only;
}

V2_float SceneInput::TransformToCamera(const V2_float& screen_position) const {
	PTGN_ASSERT(scene_ != nullptr);
	if (scene_->camera.primary != ecs::Entity{} && scene_->camera.primary.Has<impl::CameraInfo>()) {
		return scene_->camera.primary.TransformToCamera(screen_position);
	}
	return screen_position;
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