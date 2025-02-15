#include "scene/scene.h"

#include "core/game.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "math/collision.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "utility/tween.h"

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

void Scene::InternalLoad() {
	/*target_ = RenderTarget{ game.window.GetSize(), color::Transparent };

	game.event.window.Subscribe(
		WindowEvent::Resized, this,
		std::function([&](const WindowResizedEvent& e) { target_.GetTexture().Resize(e.size); })
	);*/
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
