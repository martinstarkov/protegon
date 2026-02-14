#include "runtime/scene/scene.h"

#include <memory>

#include "app/context.h"
#include "core/event/dispatcher.h"
#include "core/graphics/color.h"
#include "ecs/ecs.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_renderer.h"
#include "renderer/camera/camera.h"
#include "renderer/image/surface.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture_format.h"
#include "renderer/targets/render_target.h"
#include "runtime/ecs/components/uuid.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/scripting/scripts.h"
#include "serialization/json/fwd.h"

namespace ptgn {

SceneEventHandler::SceneEventHandler(Scene& scene) : scene_{ scene } {}

void SceneEventHandler::Emit(EventDispatcher d) {
	scene_.InternalEmit(d);
}

Scene::Scene() {}

Scene::~Scene() {
	// TODO: Fix.
	/*if (!render_target_.IsAlive()) {
		return;
	}
	PTGN_ASSERT(render_taret.IsAlive());
	render_target_.GetDisplayList().clear();
	render_target_.Destroy();
	Application::Get().render_.render_data_.render_manager.Refresh();*/
}

void Scene::AddToDisplayList(Entity entity) {
	// TODO: Fix.
	// if (!render_target_ || !render_target_.Has<impl::DisplayList>()) {
	//	return;
	//}
	// if (!IsVisible(entity) || !HasDraw(entity)) {
	//	return;
	//}
	// auto& dl{ render_target_.GetDisplayList() };
	// dl.emplace_back(entity);
}

void Scene::RemoveFromDisplayList(Entity entity) {
	// TODO: Fix.
	// if (!render_target_ || !render_target_.Has<impl::DisplayList>()) {
	//	return;
	//}
	// auto& dl{ render_target_.GetDisplayList() };
	// std::erase(dl, entity);
}

Entity Scene::CreateEntity() {
	auto entity{ Manager::CreateEntity() };
	entity.scene_ = this;
	// entity.template Add<SceneKey>(key_);
	return entity;
}

Entity Scene::CreateEntity(UUID uuid) {
	auto entity{ Manager::CreateEntity(uuid) };
	entity.scene_ = this;
	// entity.template Add<SceneKey>(key_);
	return entity;
}

Entity Scene::CreateEntity(const json& j) {
	auto entity{ Manager::CreateEntity(j) };
	entity.scene_ = this;
	// PTGN_ASSERT(entity.Has<SceneKey>(), "Scene entity created from json must have a scene key");
	return entity;
}

void Scene::ReEnter() {
	// TODO: Fix.
	// Application::Get().scene_.Enter(key_);
}

// void Scene::SetColliderColor(Color collider_color) {
//	collider_color_ = collider_color;
// }
//
// void Scene::SetColliderVisibility(bool collider_visibility) {
//	collider_visibility_ = collider_visibility;
// }
/*
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

	// TODO: Fix.
	V2_float draw_size{ render_target_.GetTextureSize() };

	PTGN_ASSERT(camera_size.BothAboveZero());

	V2_float scale{ draw_size / camera_size };

	PTGN_ASSERT(scale.BothAboveZero());

	return scale;
}

	*/

void Scene::SetBackgroundColor(Color background_color) {
	// TODO: Fix.
	// render_target_.SetClearColor(background_color);
}

Color Scene::GetBackgroundColor() const {
	// TODO: Fix.
	// return render_target_.GetClearColor();
	return {};
}

// const RenderTarget& Scene::GetRenderTarget() const {
//	return render_target_;
// }
//
// RenderTarget& Scene::GetRenderTarget() {
//	return render_target_;
// }

// SceneKey Scene::GetKey() const {
//	return key_;
// }

void Scene::Init(const std::shared_ptr<ApplicationContext>& ctx) {
	ctx_ = ctx;

	render_target_ = render_manager_.CreateEntity();
	render_target_ =
		CreateRenderTarget(*this, app().renderer, ResizeMode::DisplaySize, TextureFormat::RGBA8);
	camera		 = CreateCamera(render_manager_, app().renderer);
	fixed_camera = CreateCamera(render_manager_, app().renderer);
}

// void Scene::SetKey(const SceneKey& key) {
//	key_			 = key;
//	input.scene_key_ = key;
// }

void Scene::InternalEnter() {
	// Here instead of scene constructor because exiting a scene resets the manager, which will
	// clear the component pool vector which contains all the hooks.
	// OnConstruct<Visible>().Connect<Scene, &Scene::AddToDisplayList>(this);
	// OnDestruct<Visible>().Connect<Scene, &Scene::RemoveFromDisplayList>(this);
	// OnConstruct<impl::IDrawable>().Connect<Scene, &Scene::AddToDisplayList>(this);
	// OnDestruct<impl::IDrawable>().Connect<Scene, &Scene::RemoveFromDisplayList>(this);

	OnEnter();
	Refresh();
}

void Scene::InternalExit() {
	Refresh();
	OnExit();
	Refresh();
	// Clears component hooks.
	Reset();
	// physics = {};
	//  TODO: Fix.
	/*render_target_.ClearDisplayList();
	render_target_.Get<GameObject<Camera>>().Reset();*/
	// fixed_camera.Reset();
	Refresh();
}

void Scene::InternalDraw() {
	impl::RecalculateViewProjection(camera);
	impl::RecalculateViewProjection(fixed_camera);

	for (auto [e, _camera] : EntitiesWith<impl::Camera>()) {
		impl::RecalculateViewProjection(e);
	}

	app().renderer.ClearRenderTarget(render_target_.Get<RenderTarget>(), color::Transparent);

	for (auto [e, rt] : EntitiesWith<RenderTarget>()) {
		// TODO: Bind guard outside this loop to avoid redundant binds if multiple render targets
		// exist.
		// TODO: Fix. Clear render target with its clear color instead of transparent.
		app().renderer.ClearRenderTarget(rt, color::Transparent);
	}

	// TODO: Get rid of this.
	// auto game_size{ ctx_->renderer.GetGameSize() };
	// auto game_size{ ctx_->renderer.gl_renderer_->screen_target_.GetSize() };

	/*auto half{ game_size / 2.0f };
	ctx_->renderer.SetViewProjection(Matrix4::Orthographic(-half, half));*/

	// impl::Surface texture1_surface{ "assets/logo.png" };

	/*texture1 = ctx_->renderer.gl_renderer_->gl_->CreateTexture(
		texture1_surface.pixels.data(), GL_RGBA, GL_UNSIGNED_BYTE, texture1_surface.size, GL_RGBA
	);*/

	// ctx_->renderer.DrawRect({ 0, 0 }, game_size / 2.0f, color::Red);
	/*ctx_->renderer.DrawTexture(
		texture1, {}, ctx_->renderer.gl_renderer_->gl_->GetTextureSize(texture1)
	);*/
	// TODO: Fix.
	/*if (collider_visibility_) {
		for (auto [entity, collider] : EntitiesWith<Collider>()) {
			Application::Get().debug_.DrawShape(
				GetDrawTransform(entity), collider.shape, collider_color_, collider_line_width_,
				GetDrawOrigin(entity), entity.GetCamera()
			);
		}
	}
	Application::Get().render_.render_data_.Draw(*this);*/
}

void Scene::InternalUpdate() {
	// TODO: Fix.
	// app.render_.render_data_.ClearRenderTargets(*this);
	// app.render_.render_data_.SetDrawingTo(render_target_);

	Refresh();

	// input.Update(*this);

	// const auto invoke_scripts = [&](Manager& manager) {
	//	// TODO: Consider moving this into the Scripts class.
	//	for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
	//		scripts.InvokeActions();
	//	}
	//	manager.Refresh();
	// };

	// invoke_scripts(*this);

	/*const auto update_scripts = [&](Manager& manager) {
		for (auto [e, scripts] : manager.EntitiesWith<Scripts>()) {
			scripts.AddAction(&impl::IScript::OnUpdate);
		}

		invoke_scripts(manager);
	};*/

	// update_scripts(*this);

	OnUpdate();

	Refresh();

	// invoke_scripts(*this);

	// ParticleEmitter::Update(*this);

	// Tween::Update(*this, dt);

	// TODO: Fix.
	// impl::AnimationSystem::Update(*this);

	// Lifetime::Update(*this);

	// physics.PreCollisionUpdate(*this);

	// collision_.Update(*this);

	// physics.PostCollisionUpdate(*this);

	// invoke_scripts(*this);
}

void to_json(json& j, const Scene& scene) {
	to_json(j["manager"], static_cast<const Manager&>(scene));
	/*j["camera"]				 = scene.camera;
	j["key"]				 = scene.key_;
	j["physics"]			 = scene.physics;
	j["input"]				 = scene.input;
	j["collider_visibility"] = scene.collider_visibility_;
	j["collider_color"]		 = scene.collider_color_;*/
	// TODO: Fix.
	// j["render_target"]		 = scene.render_target_;
}

void from_json(const json& j, Scene& scene) {
	scene.Reset();

	// j.at("key").get_to(scene.key_);

	// Ensure manager is deserialized before any of the other scene systems which may reference
	// manager entities (such as the CameraManager).
	from_json(j.at("manager"), static_cast<Manager&>(scene));

	// j.at("physics").get_to(scene.physics);

	// j.at("collider_visibility").get_to(scene.collider_visibility_);
	// j.at("collider_color").get_to(scene.collider_color_);

	// j.at("input").get_to(scene.input);
	//  TODO: Fix.
	//  j.at("render_target").get_to(scene.render_target_);
}

ApplicationContext& Scene::app() {
	return *ctx_.get();
}

const ApplicationContext& Scene::app() const {
	return *ctx_.get();
}

void Scene::InternalEmit(EventDispatcher d) {
	for (auto [e, scripts] : EntitiesWith<impl::Scripts>()) {
		scripts.Emit(d);
		if (d.IsHandled()) {
			break;
		}
	}
	if (!d.IsHandled()) {
		OnEvent(d);
	}
}

} // namespace ptgn