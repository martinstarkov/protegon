#include "scene/scene.h"

#include <memory>

#include "components/common.h"
#include "components/draw.h"
#include "components/lifetime.h"
#include "components/offsets.h"
#include "components/uuid.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/script.h"
#include "events/input_handler.h"
#include "physics/collision/collision_handler.h"
#include "rendering/batching/render_data.h"
#include "rendering/graphics/vfx/light.h"
#include "rendering/graphics/vfx/particle.h"
#include "rendering/renderer.h"
#include "scene/camera.h"
#include "scene/scene_key.h"
#include "scene/scene_manager.h"
#include "tweening/tween.h"
#include "tweening/tween_effects.h"

namespace ptgn {

void Scene::Add(Action new_status) {
	actions_.insert(new_status);
}

Entity Scene::CreateEntity() {
	auto entity{ Manager::CreateEntity() };
	entity.template Add<impl::SceneKey>(key_);
	return entity;
}

Entity Scene::CreateEntity(UUID uuid) {
	auto entity{ Manager::CreateEntity(uuid) };
	entity.template Add<impl::SceneKey>(key_);
	return entity;
}

Entity Scene::CreateEntity(const json& j) {
	auto entity{ Manager::CreateEntity(j) };
	PTGN_ASSERT(
		entity.Has<impl::SceneKey>(), "Scene entity created from json must have a scene key"
	);
	return entity;
}

void Scene::SetColliderColor(const Color& collider_color) {
	collider_color_ = collider_color;
}

void Scene::SetColliderVisibility(bool collider_visibility) {
	collider_visibility_ = collider_visibility;
}

void Scene::ReEnter() {
	InternalExit();
	InternalEnter();
}

void Scene::Init() {
	active_ = true;

	// Input is reset to ensure no previously pressed keys are considered held.
	game.input.ResetKeyStates();
	game.input.ResetMouseStates();

	camera.Init(key_);
	input.Init(key_);
}

void Scene::InternalEnter() {
	Init();
	Enter();
	Refresh();
}

void Scene::InternalExit() {
	Refresh();
	Exit();
	Refresh();
	Reset();
	input.Shutdown();
	active_ = false;
	camera	= {};
	physics = {};
	Refresh();
}

void Scene::Draw() {
	if (collider_visibility_) {
		for (auto [e, b] : EntitiesWith<BoxCollider>()) {
			const auto& transform{ e.GetDrawTransform() };
			DrawDebugRect(
				transform.position, b.size, collider_color_, b.origin, 1.0f, transform.rotation
			);
		}
		for (auto [e, c] : EntitiesWith<CircleCollider>()) {
			const auto& transform{ e.GetDrawTransform() };
			DrawDebugCircle(transform.position, c.radius, collider_color_, 1.0f);
		}
	}
	auto& render_data{ game.renderer.GetRenderData() };
	render_data.Draw(*this);
}

void Scene::PreUpdate() {
	Refresh();

	input.UpdatePrevious(this);

	Refresh();

	input.UpdateCurrent(this);

	Refresh();
}

void Scene::PostUpdate() {
	Refresh();

	game.scene.current_ = game.scene.GetActiveScene(key_);

	float dt{ game.dt() };
	float time{ game.time() };

	Scripts::Update(*this, dt);

	impl::ScriptTimers::Update(*this);
	impl::ScriptRepeats::Update(*this);

	Update();

	Refresh();

	ParticleEmitter::Update(*this);

	for (auto [entity, tween] : EntitiesWith<impl::TweenInstance>()) {
		Tween{ entity }.Step(dt);
	}

	translate_effects_.Update(*this);
	rotate_effects_.Update(*this);
	scale_effects_.Update(*this);
	tint_effects_.Update(*this);
	bounce_effects_.Update(*this);
	shake_effects_.Update(*this, time, dt);
	follow_effects_.Update(*this);

	impl::AnimationSystem::Update(*this);

	Lifetime::Update(*this);

	physics.PreCollisionUpdate(*this);

	impl::CollisionHandler::Update(*this);

	physics.PostCollisionUpdate(*this);

	// TODO: Use Entity::Copy() instead. It caused some weird bug when I tried.

	PTGN_ASSERT(camera.primary.IsAlive(), "Scene must be reinitialized after clearing");

	camera.primary_unzoomed.Get<Transform>()		= camera.primary.Get<Transform>();
	camera.primary_unzoomed.Get<impl::CameraInfo>() = camera.primary.Get<impl::CameraInfo>();
	camera.window_unzoomed.Get<Transform>()			= camera.window.Get<Transform>();
	camera.window_unzoomed.Get<impl::CameraInfo>()	= camera.window.Get<impl::CameraInfo>();

	camera.primary_unzoomed.SetZoom(1.0f);
	camera.window_unzoomed.SetZoom(1.0f);

	// TODO: Update dirty vertex caches.

	Draw();

	game.scene.current_ = {};
}

void to_json(json& j, const Scene& scene) {
	to_json(j["manager"], static_cast<const Manager&>(scene));
	j["key"]				 = scene.key_;
	j["active"]				 = scene.active_;
	j["actions"]			 = scene.actions_;
	j["physics"]			 = scene.physics;
	j["input"]				 = scene.input;
	j["camera"]				 = scene.camera;
	j["collider_visibility"] = scene.collider_visibility_;
	j["collider_color"]		 = scene.collider_color_;
}

void from_json(const json& j, Scene& scene) {
	scene.Reset();

	j.at("key").get_to(scene.key_);
	j.at("active").get_to(scene.active_);
	j.at("actions").get_to(scene.actions_);

	// Ensure manager is deserialized before any of the other scene systems which may reference
	// manager entities (such as the CameraManager).
	from_json(j.at("manager"), static_cast<Manager&>(scene));

	j.at("physics").get_to(scene.physics);

	j.at("collider_visibility").get_to(scene.collider_visibility_);
	j.at("collider_color").get_to(scene.collider_color_);

	j.at("input").get_to(scene.input);
	j.at("camera").get_to(scene.camera);
}

} // namespace ptgn