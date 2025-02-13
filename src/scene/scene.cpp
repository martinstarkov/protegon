#include "scene/scene.h"

#include "core/game.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "math/collision.h"
#include "renderer/renderer.h"
#include "utility/tween.h"

namespace ptgn {

void Scene::Add(Action new_status) {
	actions_.insert(new_status);
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
	// input.Update();
	Update();
	for (auto [e, tween] : manager.EntitiesWith<Tween>()) {
		tween.Step(game.dt());
	}
	physics.PreCollisionUpdate(manager);
	impl::CollisionHandler::Update(manager);
	physics.PostCollisionUpdate(manager);
	game.renderer.GetRenderData().Render({}, camera.primary, manager);
	manager.Refresh();
}

} // namespace ptgn
