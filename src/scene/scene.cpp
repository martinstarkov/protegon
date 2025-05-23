#include "scene/scene.h"

#include "components/common.h"
#include "components/draw.h"
#include "components/lifetime.h"
#include "components/offsets.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "events/input_handler.h"
#include "physics/collision/collision.h"
#include "rendering/graphics/vfx/light.h"
#include "rendering/graphics/vfx/particle.h"
#include "rendering/renderer.h"
#include "scene/camera.h"
#include "tweening/tween.h"
#include "tweening/tween_effects.h"

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

void Scene::SetColliderColor(const Color& collider_color) {
	collider_color_ = collider_color;
}

void Scene::SetColliderVisibility(bool collider_visibility) {
	collider_visibility_ = collider_visibility;
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

void Scene::Draw() {
	if (collider_visibility_) {
		for (auto [e, b] : manager.EntitiesWith<BoxCollider>()) {
			const auto& transform{ e.GetAbsoluteTransform() };
			DrawDebugRect(
				transform.position, b.size, collider_color_, b.origin, 1.0f, transform.rotation
			);
		}
		for (auto [e, c] : manager.EntitiesWith<CircleCollider>()) {
			const auto& transform{ e.GetAbsoluteTransform() };
			DrawDebugCircle(transform.position, c.radius, collider_color_, 1.0f);
		}
	}
	auto& render_data{ game.renderer.GetRenderData() };
	render_data.Render({} /*target_.GetFrameBuffer()*/, camera.primary, manager);
	// render_data.RenderToScreen(target_, camera.primary);
}

void Scene::PostUpdate() {
	float dt{ game.dt() };
	float time{ game.time() };

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
		tween.Step(dt);
		// tween_update_count++;
	}

	translate_effects_.Update(manager);
	rotate_effects_.Update(manager);
	scale_effects_.Update(manager);
	tint_effects_.Update(manager);
	bounce_effects_.Update(manager);
	shake_effects_.Update(manager, time, dt);
	follow_effects_.Update(manager);

	impl::AnimationSystem::Update(manager);

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

	Draw();
}

} // namespace ptgn