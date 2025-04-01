#include "scene/scene.h"

#include "components/common.h"
#include "components/lifetime.h"
#include "components/offsets.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "event/input_handler.h"
#include "math/collision/collision.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "utility/tween.h"
#include "vfx/particle.h"
#include "vfx/tween_effects.h"

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

Entity Scene::CreateEntity() {
	return manager.CreateEntity();
}

void Scene::InternalEnter() {
	active_ = true;
	// Input is reset to ensure no previously pressed keys are considered held.
	game.input.ResetKeyStates();
	game.input.ResetMouseStates();
	/*target_ = RenderTarget{ game.window.GetSize(), color::Transparent };

	game.event.window.Subscribe(
		WindowEvent::Resized, this,
		std::function([&](const WindowResizedEvent& e) { target_.GetTexture().Resize(e.size); })
	);*/

	camera.Init(manager);
	Enter();
	manager.Refresh();

	input.Init(this);
	manager.Refresh();
}

void Scene::InternalExit() {
	manager.Refresh();
	Exit();
	manager.Refresh();
	manager.Reset();
	input.Shutdown();
	// game.event.window.Unsubscribe(this);
	active_ = false;
	manager.Refresh();
}

void Scene::PreUpdate() {
	manager.Refresh();
	input.UpdatePrevious();
	manager.Refresh();
	input.UpdateCurrent();
	manager.Refresh();
}

void Scene::PostUpdate() {
	// TODO: Add multiple manager support?

	manager.Refresh();
	Update();
	manager.Refresh();
	for (auto [e, enabled, particle_manager] :
		 manager.EntitiesWith<Enabled, impl::ParticleEmitterComponent>()) {
		particle_manager.Update(e.GetPosition());
	}
	// std::size_t tween_update_count{ 0 };
	for (auto [e, tween] : manager.EntitiesWith<Tween>()) {
		tween.Step(game.dt());
		// tween_update_count++;
	}
	float dt{ game.dt() };
	float time{ game.time() };
	for (auto [e, shake, offsets] : manager.EntitiesWith<impl::ShakeEffect, impl::Offsets>()) {
		shake.Update(e, dt, time);
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