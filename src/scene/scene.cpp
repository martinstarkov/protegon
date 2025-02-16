#include "scene/scene.h"

#include <functional>

#include "components/input.h"
#include "core/game.h"
#include "ecs/ecs.h"
#include "event/event.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "event/input_handler.h"
#include "event/mouse.h"
#include "math/collision.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
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
						auto p{ key };
						Invoke(callback, std::move(p));
					}
					break;
				}
				case KeyEvent::Up: {
					Key key{ static_cast<const KeyUpEvent&>(event).key };
					for (auto [e, interactive, callback] :
						 manager.EntitiesWith<Interactive, callback::KeyUp>()) {
						auto p{ key };
						Invoke(callback, std::move(p));
					}
					break;
				}
				case KeyEvent::Pressed: {
					Key key{ static_cast<const KeyPressedEvent&>(event).key };
					for (auto [e, interactive, callback] :
						 manager.EntitiesWith<Interactive, callback::KeyPressed>()) {
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
		// TODO: Implement checks depending on entity shape.
		return true;
	};

	auto GetMousePos = [&]() {
		// TODO: Replace with local input.GetMousePosition().
		return V2_float{ game.input.GetMousePosition() };
	};

	game.event.mouse.Subscribe(
		this, std::function([&](MouseEvent type, const Event& event) {
			V2_float pos{ std::invoke(GetMousePos) };
			switch (type) {
				// TODO: Add draggable events.
				case MouseEvent::Move: {
					for (auto [e, interactive] : manager.EntitiesWith<Interactive>()) {
						if (e.Has<callback::MouseMove>()) {
							V2_float p{ pos };
							Invoke(e.Get<callback::MouseMove>(), std::move(p));
						}
						// TODO: Add other mouse move events.
					}
					break;
				}
				case MouseEvent::Down: {
					Mouse mouse{ static_cast<const MouseDownEvent&>(event).mouse };
					for (auto [e, interactive] : manager.EntitiesWith<Interactive>()) {
						bool down_listener{ e.Has<callback::MouseDown>() };
						bool down_outside_listener{ e.Has<callback::MouseDownOutside>() };
						if (!down_listener && !down_outside_listener) {
							continue;
						}
						bool inside{ std::invoke(PointerIsInside, pos, e) };
						if (down_listener && inside) {
							auto p{ mouse };
							Invoke(e.Get<callback::MouseDown>(), std::move(p));
						}
						if (down_outside_listener && !inside) {
							auto p{ mouse };
							Invoke(e.Get<callback::MouseDownOutside>(), std::move(p));
						}
						// TODO: Add other mouse move events.
					}
					break;
				}
				case MouseEvent::Up: {
					Mouse mouse{ static_cast<const MouseUpEvent&>(event).mouse };
					for (auto [e, interactive] : manager.EntitiesWith<Interactive>()) {
						bool up_listener{ e.Has<callback::MouseUp>() };
						bool up_outside_listener{ e.Has<callback::MouseUpOutside>() };
						if (!up_listener && !up_outside_listener) {
							continue;
						}
						bool inside{ std::invoke(PointerIsInside, pos, e) };
						if (up_listener && inside) {
							auto p{ mouse };
							Invoke(e.Get<callback::MouseUp>(), std::move(p));
						}
						if (up_outside_listener && !inside) {
							auto p{ mouse };
							Invoke(e.Get<callback::MouseUpOutside>(), std::move(p));
						}
						// TODO: Add other mouse move events.
					}
					break;
				}
				case MouseEvent::Pressed: {
					Mouse mouse{ static_cast<const MousePressedEvent&>(event).mouse };
					for (auto [e, interactive, callback] :
						 manager.EntitiesWith<Interactive, callback::MousePressed>()) {
						if (std::invoke(PointerIsInside, pos, e)) {
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
						if (std::invoke(PointerIsInside, pos, e)) {
							auto p{ scroll };
							Invoke(callback, std::move(p));
						}
					}
					break;
				}
				default: PTGN_ERROR("Unimplemented mouse event type");
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
	physics.PreCollisionUpdate(manager);
	impl::CollisionHandler::Update(manager);
	physics.PostCollisionUpdate(manager);
	manager.Refresh();
	auto& render_data{ game.renderer.GetRenderData() };
	render_data.Render({} /*target_.GetFrameBuffer()*/, camera.primary, manager);

	// render_data.RenderToScreen(target_, camera.primary);
}

} // namespace ptgn
