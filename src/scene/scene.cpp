#include "scene/scene.h"

#include "components/lifetime.h"
#include "core/game.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "math/collision/collision.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "utility/tween.h"

namespace ptgn {

/*
void Scene::ClearTarget() {
	target_.Bind();
	target_.Clear();
}
*/

void Scene::Add(Action new_status) {
	actions_.insert(new_status);
}

void Scene::InternalEnter() {
	active_ = true;
	// Input is reset to ensure no previously pressed keys are considered held.
	game.input.ResetKeyStates();
	game.input.ResetMouseStates();

	input.Init(this);

	/*target_ = RenderTarget{ game.window.GetSize(), color::Transparent };

	game.event.window.Subscribe(
		WindowEvent::Resized, this,
		std::function([&](const WindowResizedEvent& e) { target_.GetTexture().Resize(e.size); })
	);*/

	camera.Init(manager);
	Enter();
	manager.Refresh();
}

void Scene::InternalExit() {
	Exit();
	manager.Refresh();
	manager.Reset();
	input.Shutdown();
	// game.event.window.Unsubscribe(this);
	active_ = false;
}

void Scene::InternalUpdate() {
	manager.Refresh();
	input.Update();
	manager.Refresh();
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