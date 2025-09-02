#include "scene/scene.h"

#include <vector>

#include "common/assert.h"
#include "components/animation.h"
#include "components/draw.h"
#include "components/drawable.h"
#include "components/lifetime.h"
#include "components/transform.h"
#include "components/uuid.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/game_object.h"
#include "core/manager.h"
#include "core/script.h"
#include "core/script_interfaces.h"
#include "ecs/ecs.h"
#include "input/input_handler.h"
#include "math/geometry.h"
#include "math/vector2.h"
#include "nlohmann/json.hpp"
#include "physics/collision/collider.h"
#include "physics/collision/collision_handler.h"
#include "physics/physics.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/render_data.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "renderer/vfx/particle.h"
#include "scene/camera.h"
#include "scene/scene_input.h"
#include "scene/scene_key.h"
#include "scene/scene_manager.h"
#include "serialization/fwd.h"
#include "tweens/tween.h"
#include "utility/flags.h"

namespace ptgn {

Scene::Scene() {
	auto& render_manager{ game.renderer.GetRenderData().render_manager };
	render_target_ = CreateRenderTarget(
		render_manager, ResizeMode::DisplaySize, color::Transparent, TextureFormat::RGBA8888
	);
	PTGN_ASSERT(render_target_.Has<GameObject<Camera>>());
	camera		 = render_target_.Get<GameObject<Camera>>();
	fixed_camera = CreateCamera(*this);
	SetBlendMode(render_target_, BlendMode::Blend);
}

Scene::~Scene() {
	if (!render_target_.IsAlive()) {
		return;
	}
	render_target_.GetDisplayList().clear();
	render_target_.Destroy();
	game.renderer.GetRenderData().render_manager.Refresh();
}

void Scene::AddToDisplayList(Entity entity) {
	if (!render_target_ || !render_target_.Has<impl::DisplayList>()) {
		return;
	}
	if (!IsVisible(entity) || !HasDraw(entity)) {
		return;
	}
	auto& dl{ render_target_.GetDisplayList() };
	dl.emplace_back(entity);
}

void Scene::RemoveFromDisplayList(Entity entity) {
	if (!render_target_ || !render_target_.Has<impl::DisplayList>()) {
		return;
	}
	auto& dl{ render_target_.GetDisplayList() };
	std::erase(dl, entity);
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

void Scene::ReEnter() {
	game.scene.Enter(key_);
}

void Scene::SetColliderColor(const Color& collider_color) {
	collider_color_ = collider_color;
}

void Scene::SetColliderVisibility(bool collider_visibility) {
	collider_visibility_ = collider_visibility;
}

V2_float Scene::GetScaleRelativeTo(const Camera& relative_to_camera) const {
	auto cam{ relative_to_camera ? relative_to_camera : camera };
	// auto camera_zoom{ cam.GetZoom() };
	// PTGN_ASSERT(camera_zoom.BothAboveZero());
	V2_float camera_size{ cam.GetViewportSize() };
	// Not accounting for camera zoom because otherwise text scaling becomes jittery.
	// camera_size /= camera_zoom;
	V2_float draw_size{ render_target_.GetTextureSize() };
	PTGN_ASSERT(camera_size.BothAboveZero());
	V2_float scale{ draw_size / camera_size };
	PTGN_ASSERT(scale.BothAboveZero());
	return scale;
}

void Scene::SetBackgroundColor(const Color& background_color) {
	render_target_.SetClearColor(background_color);
}

Color Scene::GetBackgroundColor() const {
	return render_target_.GetClearColor();
}

const RenderTarget& Scene::GetRenderTarget() const {
	return render_target_;
}

RenderTarget& Scene::GetRenderTarget() {
	return render_target_;
}

impl::SceneKey Scene::GetKey() const {
	return key_;
}

void Scene::Init() {
	render_target_.Get<GameObject<Camera>>().Reset();
	fixed_camera.Reset();
}

void Scene::SetKey(const impl::SceneKey& key) {
	key_			 = key;
	input.scene_key_ = key;
}

void Scene::InternalEnter() {
	// Here instead of scene constructor because exiting a scene resets the manager, which will
	// clear the component pool vector which contains all the hooks.
	OnConstruct<Visible>().Connect<Scene, &Scene::AddToDisplayList>(this);
	OnDestruct<Visible>().Connect<Scene, &Scene::RemoveFromDisplayList>(this);
	OnConstruct<impl::IDrawable>().Connect<Scene, &Scene::AddToDisplayList>(this);
	OnDestruct<impl::IDrawable>().Connect<Scene, &Scene::RemoveFromDisplayList>(this);

	Init();
	Enter();
	Refresh();
}

void Scene::InternalExit() {
	Refresh();
	Exit();
	Refresh();
	// Clears component hooks.
	Reset();
	physics = {};
	render_target_.ClearDisplayList();
	render_target_.Get<GameObject<Camera>>().Reset();
	fixed_camera.Reset();
	Refresh();
}

void Scene::InternalDraw() {
	if (collider_visibility_) {
		for (auto [entity, collider] : EntitiesWith<Collider>()) {
			auto transform{ GetDrawTransform(entity) };
			transform = ApplyOffset(collider.shape, transform, entity);
			DrawDebugShape(
				transform, collider.shape, collider_color_, collider_line_width_, entity.GetCamera()
			);
		}
	}
	auto& render_data{ game.renderer.GetRenderData() };
	render_data.Draw(*this);
}

void Scene::InternalUpdate() {
	auto& render_data{ game.renderer.GetRenderData() };
	render_data.ClearRenderTargets(*this);
	render_data.drawing_to_ = impl::RenderData::GetDrawTarget(*this);

	Refresh();

	game.input.InvokeInputEvents(*this);

	input.Update(*this);

	const auto invoke_scripts = [&](Manager& manager) {
		// TODO: Consider moving this into the Scripts class.
		for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
			scripts.InvokeActions();
		}
		manager.Refresh();
	};

	invoke_scripts(*this);

	float dt{ game.dt() };

	const auto update_scripts = [&](Manager& manager) {
		for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
			scripts.AddAction(&impl::IScript::OnUpdate);
		}

		invoke_scripts(manager);
	};

	update_scripts(*this);

	Update();

	Refresh();

	invoke_scripts(*this);

	ParticleEmitter::Update(*this);

	Tween::Update(*this, dt);

	impl::AnimationSystem::Update(*this);

	Lifetime::Update(*this);

	physics.PreCollisionUpdate(*this);

	collision_.Update(*this);

	physics.PostCollisionUpdate(*this);

	invoke_scripts(*this);

	// TODO: Update dirty vertex caches.

	InternalDraw();

	for (auto [entity, transform] : InternalEntitiesWith<Transform>()) {
		transform.ClearDirtyFlags();
	}
}

void to_json(json& j, const Scene& scene) {
	to_json(j["manager"], static_cast<const Manager&>(scene));
	j["camera"]				 = scene.camera;
	j["key"]				 = scene.key_;
	j["physics"]			 = scene.physics;
	j["input"]				 = scene.input;
	j["collider_visibility"] = scene.collider_visibility_;
	j["collider_color"]		 = scene.collider_color_;
	j["render_target"]		 = scene.render_target_;
}

void from_json(const json& j, Scene& scene) {
	scene.Reset();

	j.at("key").get_to(scene.key_);

	// Ensure manager is deserialized before any of the other scene systems which may reference
	// manager entities (such as the CameraManager).
	from_json(j.at("manager"), static_cast<Manager&>(scene));

	j.at("physics").get_to(scene.physics);

	j.at("collider_visibility").get_to(scene.collider_visibility_);
	j.at("collider_color").get_to(scene.collider_color_);

	j.at("input").get_to(scene.input);
	j.at("render_target").get_to(scene.render_target_);
}

} // namespace ptgn