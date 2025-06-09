#include "scene/scene.h"

#include <memory>

#include "components/common.h"
#include "components/draw.h"
#include "components/lifetime.h"
#include "components/offsets.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/script.h"
#include "events/input_handler.h"
#include "physics/collision/collision.h"
#include "rendering/graphics/vfx/light.h"
#include "rendering/graphics/vfx/particle.h"
#include "rendering/renderer.h"
#include "scene/camera.h"
#include "scene/scene_manager.h"
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

void Scene::Init() {
	active_ = true;
	// Input is reset to ensure no previously pressed keys are considered held.
	game.input.ResetKeyStates();
	game.input.ResetMouseStates();
	/*target_ = RenderTarget{ game.window.GetSize(), color::Transparent };

	game.event.window.Subscribe(
		WindowEvent::Resized, this,
		std::function([&](const WindowResizedEvent& e) { target_.GetTexture().Resize(e.size); })
	);*/

	camera.Init(key_);
	input.Init(key_);
}

void Scene::InternalEnter() {
	Init();
	Enter();
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
	render_data.Render({} /*target_.GetFrameBuffer()*/, manager);
	// render_data.RenderToScreen(target_, camera.primary);
}

void Scene::PreUpdate(Manager& m) {
	m.Refresh();

	input.UpdatePrevious(this);

	m.Refresh();

	input.UpdateCurrent(this);

	m.Refresh();
}

void Scene::PostUpdate(Manager& m) {
	// TODO: Add multiple manager support?

	m.Refresh();

	game.scene.current_ = game.scene.GetActiveScene(key_);

	float dt{ game.dt() };
	float time{ game.time() };

	Scripts::Update(m, dt);

	impl::ScriptTimers::Update(m);
	impl::ScriptRepeats::Update(manager);

	Update();

	m.Refresh();

	ParticleEmitter::Update(m);

	for (auto [entity, tween] : m.EntitiesWith<impl::TweenInstance>()) {
		Tween{ entity }.Step(dt);
	}

	translate_effects_.Update(m);
	rotate_effects_.Update(m);
	scale_effects_.Update(m);
	tint_effects_.Update(m);
	bounce_effects_.Update(m);
	shake_effects_.Update(m, time, dt);
	follow_effects_.Update(m);

	impl::AnimationSystem::Update(m);

	Lifetime::Update(m);

	physics.PreCollisionUpdate(m);

	impl::CollisionHandler::Update(m);

	physics.PostCollisionUpdate(m);

	Draw();

	game.scene.current_ = {};
}

void to_json(json& j, const Scene& scene) {
	j["key"]				 = scene.key_;
	j["active"]				 = scene.active_;
	j["actions"]			 = scene.actions_;
	j["manager"]			 = scene.manager;
	j["physics"]			 = scene.physics;
	j["input"]				 = scene.input;
	j["camera"]				 = scene.camera;
	j["collider_visibility"] = scene.collider_visibility_;
	j["collider_color"]		 = scene.collider_color_;
}

void from_json(const json& j, Scene& scene) {
	scene.manager.Reset();

	j.at("key").get_to(scene.key_);
	j.at("active").get_to(scene.active_);
	j.at("actions").get_to(scene.actions_);

	// Ensure manager is deserialized before any of the other scene systems which may reference
	// manager entities (such as the CameraManager).
	j.at("manager").get_to(scene.manager);

	j.at("physics").get_to(scene.physics);

	j.at("collider_visibility").get_to(scene.collider_visibility_);
	j.at("collider_color").get_to(scene.collider_color_);

	j.at("input").get_to(scene.input);
	j.at("camera").get_to(scene.camera);
}

} // namespace ptgn