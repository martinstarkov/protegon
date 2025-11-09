#include "world/scene/scene.h"

#include <vector>

#include "core/app/application.h"
#include "core/app/manager.h"
#include "core/assert.h"
#include "core/ecs/components/animation.h"
#include "core/ecs/components/draw.h"
#include "core/ecs/components/drawable.h"
#include "core/ecs/components/lifetime.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/components/uuid.h"
#include "core/ecs/entity.h"
#include "core/ecs/game_object.h"
#include "core/input/input_handler.h"
#include "core/scripting/script.h"
#include "core/scripting/script_interfaces.h"
#include "core/util/flags.h"
#include "debug/debug_system.h"
#include "ecs/ecs.h"
#include "math/geometry_utils.h"
#include "math/vector2.h"
#include "nlohmann/json.hpp"
#include "physics/collider.h"
#include "physics/collision_handler.h"
#include "physics/physics.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/material/texture.h"
#include "renderer/render_data.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/vfx/particle.h"
#include "serialization/json/fwd.h"
#include "tween/tween.h"
#include "world/scene/camera.h"
#include "world/scene/scene_input.h"
#include "world/scene/scene_key.h"
#include "world/scene/scene_manager.h"

namespace ptgn {

Scene::Scene() {
	auto& app{ Application::Get() };
	auto& render_manager{ app.render_.render_data_.render_manager };
	render_target_ = CreateRenderTarget(
		render_manager, ResizeMode::DisplaySize, true, color::Transparent, TextureFormat::RGBA8888
	);
	PTGN_ASSERT(render_target_.Has<GameObject<Camera>>());
	camera		 = render_target_.Get<GameObject<Camera>>();
	fixed_camera = CreateCamera(render_manager);
	SetBlendMode(render_target_, BlendMode::Blend);
}

Scene::~Scene() {
	if (!render_target_.IsAlive()) {
		return;
	}
	render_target_.GetDisplayList().clear();
	render_target_.Destroy();
	Application::Get().render_.render_data_.render_manager.Refresh();
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
	entity.template Add<SceneKey>(key_);
	return entity;
}

Entity Scene::CreateEntity(UUID uuid) {
	auto entity{ Manager::CreateEntity(uuid) };
	entity.template Add<SceneKey>(key_);
	return entity;
}

Entity Scene::CreateEntity(const json& j) {
	auto entity{ Manager::CreateEntity(j) };
	PTGN_ASSERT(entity.Has<SceneKey>(), "Scene entity created from json must have a scene key");
	return entity;
}

void Scene::ReEnter() {
	// TODO: Fix.
	// Application::Get().scene_.Enter(key_);
}

void Scene::SetColliderColor(const Color& collider_color) {
	collider_color_ = collider_color;
}

void Scene::SetColliderVisibility(bool collider_visibility) {
	collider_visibility_ = collider_visibility;
}

V2_float Scene::GetCameraScaleRelativeTo(const Camera& relative_to_camera) const {
	if (!relative_to_camera) {
		return { 1.0f, 1.0f };
	}

	V2_float camera_size{ relative_to_camera.GetViewportSize() };

	V2_float primary_camera_size{ camera.GetViewportSize() };

	PTGN_ASSERT(camera_size.BothAboveZero());

	V2_float scale{ primary_camera_size / camera_size };

	PTGN_ASSERT(scale.BothAboveZero());

	return scale;
}

V2_float Scene::GetRenderTargetScaleRelativeTo(const Camera& relative_to_camera) const {
	auto cam{ relative_to_camera ? relative_to_camera : camera };

	V2_float camera_size{ cam.GetViewportSize() };

	// auto camera_zoom{ cam.GetZoom() };
	// PTGN_ASSERT(camera_zoom.BothAboveZero());
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

SceneKey Scene::GetKey() const {
	return key_;
}

void Scene::Init() {
	render_target_.Get<GameObject<Camera>>().Reset();
	fixed_camera.Reset();
}

void Scene::SetKey(const SceneKey& key) {
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
			Application::Get().debug_.DrawShape(
				GetDrawTransform(entity), collider.shape, collider_color_, collider_line_width_,
				GetDrawOrigin(entity), entity.GetCamera()
			);
		}
	}
	Application::Get().render_.render_data_.Draw(*this);
}

void Scene::InternalUpdate(Application& app) {
	app.render_.render_data_.ClearRenderTargets(*this);
	app.render_.render_data_.SetDrawingTo(render_target_);

	Refresh();

	app.input_.InvokeInputEvents(app, *this);

	input.Update(*this);

	const auto invoke_scripts = [&](Manager& manager) {
		// TODO: Consider moving this into the Scripts class.
		for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
			scripts.InvokeActions();
		}
		manager.Refresh();
	};

	invoke_scripts(*this);

	float dt{ app.dt() };

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