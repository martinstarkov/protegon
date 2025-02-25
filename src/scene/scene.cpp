#include "scene/scene.h"

#include <functional>
#include <type_traits>

#include "components/draw.h"
#include "components/input.h"
#include "components/lifetime.h"
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
#include "math/collision/collision.h"
#include "math/collision/overlap.h"
#include "math/geometry/circle.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "utility/assert.h"
#include "utility/log.h"
#include "utility/tween.h"
#include "utility/utility.h"

namespace ptgn {

void Scene::Add(Action new_status) {
	actions_.insert(new_status);
}

/*
void Scene::ClearTarget() {
	target_.Bind();
	target_.Clear();
}
*/

void Scene::InternalUnload() {
	game.event.key.Unsubscribe(this);
	game.event.mouse.Unsubscribe(this);
}

void Scene::InternalLoad() {
	/*target_ = RenderTarget{ game.window.GetSize(), color::Transparent };

	game.event.window.Subscribe(
		WindowEvent::Resized, this,
		std::function([&](const WindowResizedEvent& e) { target_.GetTexture().Resize(e.size); })
	);*/

	game.event.key.Subscribe(
		this, std::function([&](KeyEvent type, const Event& event) {
			// TODO: Move this function elsewhere.
			switch (type) {
				case KeyEvent::Down: {
					Key key{ static_cast<const KeyDownEvent&>(event).key };
					for (auto [e, interactive, callback] :
						 manager.EntitiesWith<Interactive, callback::KeyDown>()) {
						if (!interactive.enabled) {
							continue;
						}
						auto p{ key };
						Invoke(callback, std::move(p));
					}
					break;
				}
				case KeyEvent::Up: {
					Key key{ static_cast<const KeyUpEvent&>(event).key };
					for (auto [e, interactive, callback] :
						 manager.EntitiesWith<Interactive, callback::KeyUp>()) {
						if (!interactive.enabled) {
							continue;
						}
						auto p{ key };
						Invoke(callback, std::move(p));
					}
					break;
				}
				case KeyEvent::Pressed: {
					Key key{ static_cast<const KeyPressedEvent&>(event).key };
					for (auto [e, interactive, callback] :
						 manager.EntitiesWith<Interactive, callback::KeyPressed>()) {
						if (!interactive.enabled) {
							continue;
						}
						auto p{ key };
						Invoke(callback, std::move(p));
					}
					break;
				}
				default: PTGN_ERROR("Unimplemented key event type");
			}
		})
	);

	auto PointerIsInside = [](const V2_float& pointer, ecs::Entity entity) {
		// TODO: Move this function elsewhere.
		bool is_circle{ entity.Has<InteractiveRadius>() };
		bool is_rect{ entity.Has<InteractiveSize>() };
		PTGN_ASSERT(
			!(is_rect && is_circle),
			"Entity cannot have both an interactive radius and an interactive size"
		);
		if (is_rect) {
			V2_float interactive_size{ entity.Get<InteractiveSize>() };
			auto size{ interactive_size * GetScale(entity) };
			auto half{ size / 2.0f };
			auto center{ GetPosition(entity) - GetOffsetFromCenter(size, GetOrigin(entity)) };
			return OverlapPointRect(
				pointer, center - half, center + half, GetRotation(entity),
				GetRotationCenter(entity)
			);
		} else if (is_circle) {
			float interactive_radius{ entity.Get<InteractiveRadius>() };
			auto radius{ interactive_radius * GetScale(entity).x };
			return OverlapPointCircle(pointer, GetPosition(entity), radius);
		}
		is_circle = entity.Has<Circle>();
		is_rect	  = entity.Has<Rect>();
		// Prioritize circle interactable.
		if (is_circle) {
			const auto& c{ entity.Get<Circle>() };
			return OverlapPointCircle(pointer, c.GetCenter(), c.GetRadius());
		} else if (is_rect) {
			const auto& r{ entity.Get<Rect>() };
			auto [rect_min, rect_max] = r.GetExtents();
			return OverlapPointRect(
				pointer, rect_min, rect_max, r.GetRotation(), r.GetRotationCenter()
			);
		}

		if (entity.Has<TextureKey>()) {
			const auto& texture_key{ entity.Get<TextureKey>() };
			auto size{ game.texture.GetSize(texture_key) * GetScale(entity) };
			auto half{ size / 2.0f };
			auto center{ GetPosition(entity) - GetOffsetFromCenter(size, GetOrigin(entity)) };
			return OverlapPointRect(
				pointer, center - half, center + half, GetRotation(entity),
				GetRotationCenter(entity)
			);
		}

		return false;
	};

	auto GetMousePos = [&]() {
		// TODO: Replace with local input.GetMousePosition().
		return V2_float{ game.input.GetMousePosition() };
	};

	game.event.mouse.Subscribe(
		this, std::function([&](MouseEvent type, const Event& event) {
			// TODO: Move this function elsewhere.
			// TODO: cache interactive entity list every frame to avoid repeated calls for each
			// mouse and keyboard event type.
			V2_float pos{ std::invoke(GetMousePos) };
			for (auto [e, interactive] : manager.EntitiesWith<Interactive>()) {
				if (!interactive.enabled) {
					interactive.is_inside  = false;
					interactive.was_inside = false;
				} else {
					interactive.is_inside = std::invoke(PointerIsInside, pos, e);
				}
			}
			switch (type) {
				case MouseEvent::Move: {
					for (auto [e, interactive] : manager.EntitiesWith<Interactive>()) {
						if (!interactive.enabled) {
							continue;
						}
						if (e.Has<callback::MouseMove>()) {
							V2_float p{ pos };
							Invoke(e.Get<callback::MouseMove>(), std::move(p));
						}
						bool entered{ interactive.is_inside && !interactive.was_inside };
						bool exited{ !interactive.is_inside && interactive.was_inside };
						if (e.Has<callback::MouseEnter>() && entered) {
							V2_float p{ pos };
							Invoke(e.Get<callback::MouseEnter>(), std::move(p));
						}
						if (e.Has<callback::MouseLeave>() && exited) {
							V2_float p{ pos };
							Invoke(e.Get<callback::MouseLeave>(), std::move(p));
						}
						if (e.Has<callback::MouseOver>() && interactive.is_inside) {
							V2_float p{ pos };
							Invoke(e.Get<callback::MouseOver>(), std::move(p));
						}
						if (e.Has<callback::MouseOut>() && !interactive.is_inside) {
							V2_float p{ pos };
							Invoke(e.Get<callback::MouseOut>(), std::move(p));
						}
						if (e.Has<Draggable>() && e.Get<Draggable>().dragging) {
							if (e.Has<callback::Drag>()) {
								V2_float p{ pos };
								Invoke(e.Get<callback::Drag>(), std::move(p));
							}
							if (interactive.is_inside) {
								if (e.Has<callback::DragOver>()) {
									V2_float p{ pos };
									Invoke(e.Get<callback::DragOver>(), std::move(p));
								}
								if (!interactive.was_inside && e.Has<callback::DragEnter>()) {
									V2_float p{ pos };
									Invoke(e.Get<callback::DragEnter>(), std::move(p));
								}
							} else {
								if (e.Has<callback::DragOut>()) {
									V2_float p{ pos };
									Invoke(e.Get<callback::DragOut>(), std::move(p));
								}
								if (interactive.was_inside && e.Has<callback::DragLeave>()) {
									V2_float p{ pos };
									Invoke(e.Get<callback::DragLeave>(), std::move(p));
								}
							}
						}
					}
					break;
				}
				case MouseEvent::Down: {
					Mouse mouse{ static_cast<const MouseDownEvent&>(event).mouse };
					for (auto [e, interactive] : manager.EntitiesWith<Interactive>()) {
						if (!interactive.enabled) {
							continue;
						}
						if (interactive.is_inside) {
							if (e.Has<callback::MouseDown>()) {
								auto p{ mouse };
								Invoke(e.Get<callback::MouseDown>(), std::move(p));
							}
							if (e.Has<Draggable>()) {
								if (auto& draggable{ e.Get<Draggable>() }; !draggable.dragging) {
									draggable.dragging = true;
									// TODO: Add camera.
									draggable.start	 = pos;
									draggable.offset = GetPosition(e) - draggable.start;
									draggable.target = e;
									if (e.Has<callback::DragStart>()) {
										auto p{ pos };
										Invoke(e.Get<callback::DragStart>(), std::move(p));
									}
								}
							}
						} else {
							if (e.Has<callback::MouseDownOutside>()) {
								auto p{ mouse };
								Invoke(e.Get<callback::MouseDownOutside>(), std::move(p));
							}
						}
					}
					break;
				}
				case MouseEvent::Up: {
					Mouse mouse{ static_cast<const MouseUpEvent&>(event).mouse };
					for (auto [e, interactive] : manager.EntitiesWith<Interactive>()) {
						if (!interactive.enabled) {
							continue;
						}
						if (interactive.is_inside) {
							if (e.Has<callback::MouseUp>()) {
								auto p{ mouse };
								Invoke(e.Get<callback::MouseUp>(), std::move(p));
							}
						} else {
							if (e.Has<callback::MouseUpOutside>()) {
								auto p{ mouse };
								Invoke(e.Get<callback::MouseUpOutside>(), std::move(p));
							}
						}
						if (e.Has<Draggable>()) {
							if (auto& draggable{ e.Get<Draggable>() }; draggable.dragging) {
								draggable.dragging = false;
								draggable.offset   = {};
								draggable.target   = ecs::null;
								if (e.Has<callback::DragStop>()) {
									auto p{ pos };
									Invoke(e.Get<callback::DragStop>(), std::move(p));
								}
							}
						}
					}
					break;
				}
				case MouseEvent::Pressed: {
					Mouse mouse{ static_cast<const MousePressedEvent&>(event).mouse };
					for (auto [e, interactive, callback] :
						 manager.EntitiesWith<Interactive, callback::MousePressed>()) {
						if (!interactive.enabled) {
							continue;
						}
						if (interactive.is_inside) {
							auto p{ mouse };
							Invoke(callback, std::move(p));
						}
					}
					break;
				}
				case MouseEvent::Scroll: {
					V2_int scroll{ static_cast<const MouseScrollEvent&>(event).scroll };
					for (auto [e, interactive, callback] :
						 manager.EntitiesWith<Interactive, callback::MouseScroll>()) {
						if (!interactive.enabled) {
							continue;
						}
						if (interactive.is_inside) {
							auto p{ scroll };
							Invoke(callback, std::move(p));
						}
					}
					break;
				}
				default: PTGN_ERROR("Unimplemented mouse event type");
			}
			for (auto [e, interactive] : manager.EntitiesWith<Interactive>()) {
				if (!interactive.enabled) {
					continue;
				}
				interactive.was_inside = interactive.is_inside;
			}
		})
	);
}

void Scene::InternalEnter() {
	active_ = true;
	// Input is reset to ensure no previously pressed keys are considered held.
	game.input.ResetKeyStates();
	game.input.ResetMouseStates();
	camera.Init(manager);
	Enter();
	manager.Refresh();
}

void Scene::InternalExit() {
	Exit();
	manager.Refresh();
	manager.Reset();
	active_ = false;
}

void Scene::InternalUpdate() {
	manager.Refresh();
	// input.Update();
	Update();
	manager.Refresh();
	// std::size_t tween_update_count{ 0 };
	for (auto [e, tween] : manager.EntitiesWith<Tween>()) {
		tween.Step(game.dt());
		// tween_update_count++;
	}
	// PTGN_LOG("Scene ", key_, " updated ", tween_update_count, " tweens this frame");
	manager.Refresh();
	// std::size_t lifetime_update_count{ 0 };
	for (auto [e, lifetime] : manager.EntitiesWith<Lifetime>()) {
		lifetime.Update(e);
		// lifetime_update_count++;
	}
	// PTGN_LOG("Scene ", key_, " updated ", lifetime_update_count, " lifetimes this frame");
	manager.Refresh();
	physics.PreCollisionUpdate(manager);
	manager.Refresh();
	impl::CollisionHandler::Update(manager);
	manager.Refresh();
	physics.PostCollisionUpdate(manager);
	manager.Refresh();
	auto& render_data{ game.renderer.GetRenderData() };
	render_data.Render({} /*target_.GetFrameBuffer()*/, camera.primary, manager);

	// render_data.RenderToScreen(target_, camera.primary);
}

} // namespace ptgn
