#include "runtime/scene/scene.h"

#include <memory>
#include <utility>
#include <vector>

#include "app/context.h"
#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "ecs/ecs.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_renderer.h"
#include "renderer/camera/camera.h"
#include "renderer/camera/viewport.h"
#include "renderer/image/surface.h"
#include "renderer/renderer.h"
#include "renderer/resources/render_target.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/vertex.h"
#include "runtime/ecs/components/camera_component.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/drawable.h"
#include "runtime/ecs/components/render_target_component.h"
#include "runtime/ecs/components/transform_component.h"
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

Scene::Scene() : events{ *this }, input{ *this } {}

Scene::~Scene() {
	// TODO: Fix.
	/*if (!render_target_.IsAlive()) {
		return;
	}
	PTGN_ASSERT(render_taret.IsAlive());
	render_target_.Destroy();
	Application::Get().render_.render_data_.render_manager.Refresh();*/
}

void Scene::Init(const std::shared_ptr<ApplicationContext>& ctx) {
	ctx_ = ctx;

	input.Init(ctx_);

	auto& renderer{ app().renderer };

	render_target_ = impl::CreateRenderTarget(
		render_manager_.CreateEntity(), renderer, ResizeMode::DisplaySize, TextureFormat::RGBA8
	);
	camera		 = impl::CreateCamera(render_manager_.CreateEntity(), renderer);
	fixed_camera = impl::CreateCamera(render_manager_.CreateEntity(), renderer);
	// PTGN_LOG("[scene=", this, "]");
	// PTGN_LOG("[rt=", render_target_, "]");
	// PTGN_LOG("[camera=", camera, "]");
	// PTGN_LOG("[fixed_camera=", fixed_camera, "]");

	render_manager_.Refresh();
}

void Scene::InternalEnter() {
	OnEnter();
	Refresh();
}

void Scene::InternalEmit(EventDispatcher d) {
	for (auto [e, scripts] : render_manager_.EntitiesWith<impl::Scripts>()) {
		scripts.Emit(d);
		if (d.IsHandled()) {
			return;
		}
	}
	for (auto [e, scripts] : EntitiesWith<impl::Scripts>()) {
		scripts.Emit(d);
		if (d.IsHandled()) {
			return;
		}
	}
	if (!d.IsHandled()) {
		OnEvent(d);
	}
}

static void InvokeDrawable(Renderer& renderer, Entity entity) {
	PTGN_ASSERT(entity.Has<impl::IDrawable>(), "Cannot render entity without drawable component");

	const auto& drawable{ entity.Get<impl::IDrawable>() };

	const auto& drawable_functions{ impl::IDrawable::data() };

	PTGN_ASSERT(drawable_functions.contains(drawable.hash), "Failed to identify drawable hash");

	const auto& draw_function{ drawable_functions.find(drawable.hash)->second };

	draw_function(renderer, entity);
}

template <typename F>
static void DrawDisplayList(
	Renderer& renderer, RenderTarget& rt, std::vector<Entity>& display_list, F&& filter
) {
	// Must be sorted here so that depth and creation order is accounted for.
	SortByDepth(display_list, true);

	rt.Bind();

	for (const auto& entity : display_list) {
		if (filter && filter(entity)) {
			continue;
		}
		renderer.SetBlend(GetBlendMode(entity));
		InvokeDrawable(renderer, entity);
	}
}

void Scene::InternalDraw() {
	impl::RecalculateCameraViewProjection(camera);
	impl::RecalculateCameraViewProjection(fixed_camera);

	for (auto [e, _camera] : EntitiesWith<impl::Camera>()) {
		impl::RecalculateCameraViewProjection(e);
	}

	auto& renderer{ app().renderer };

	render_target_.Get<RenderTarget>().Clear(color::Transparent);

	// PTGN_LOG("Scene target size: ", render_target_.Get<RenderTarget>().GetSize());

	for (auto [e, rt] : EntitiesWith<RenderTarget>()) {
		// TODO: Bind guard outside this loop to avoid redundant binds if multiple render targets
		// exist.
		// TODO: Fix. Clear render target with its clear color instead of transparent.
		rt.Clear(color::Transparent);
	}

	// Loop through render targets and render their display lists onto their internal frame
	// buffers.
	for (auto [entity, visible, drawable, rt, display_list] :
		 EntitiesWith<impl::Visible, impl::IDrawable, RenderTarget>()) {
		DrawDisplayList(renderer, rt, display_list.entities, [](Entity) { return false; });
	}

	DrawDisplayList(renderer, render_target_.Get<RenderTarget>(), [](Entity entity) {
		// Skip entities which are in the display list of a custom render target.
		return entity.Has<RenderTarget>();
	});

	renderer.GetScreenTarget().Bind(*app().renderer.gl_renderer_->gl);

	auto half_viewport{ renderer.GetGameSize() * 0.5f };
	renderer.SetViewProjection(Matrix4::Orthographic(-half_viewport, half_viewport));
	renderer.SetBlend(BlendMode::Blend);

	renderer.DrawQuadTexture(
		render_target_.Get<RenderTarget>(),
		impl::GetCenteredQuadPoints(render_target_.Get<RenderTarget>().GetSize()),
		GetTint(render_target_), true
	);

	// for (auto [e, handle] : EntitiesWith<Handle<Asset::Texture>>()) {
	//	PTGN_LOG(
	//		"Entity ", e.GetHash(), ", texture size: ", renderer.GetTextureSize(handle.Get())
	//	);
	// }

	// TODO: Draw render target display lists to their render targets.
	// TODO: Draw display list to render target.
	// TODO: Bind render target blend mode.
	// TODO: Draw render target to screen target.

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

	input.Update();

	Refresh();
	OnUpdate();
	Refresh();

	// TODO: Fix.
	// ParticleEmitter::Update(*this);
	// Tween::Update(*this, dt);
	// impl::AnimationSystem::Update(*this);
	// Lifetime::Update(*this);
	// physics.PreCollisionUpdate(*this);
	// collision_.Update(*this);
	// physics.PostCollisionUpdate(*this);
}

void Scene::InternalExit() {
	Refresh();
	OnExit();
	Refresh();
	// Clears component hooks.
	manager_.Reset();
	// physics = {};
	//  TODO: Fix.
	// render_target_.Get<GameObject<Camera>>().Reset();
	// fixed_camera.Reset();
	Refresh();
}

// void Scene::ReEnter() {
//	// TODO: Fix.
//	// Application::Get().scene_.Enter(key_);
// }

V2_float Scene::GetCameraScaleRelativeTo(Entity relative_to_camera) const {
	if (!relative_to_camera) {
		return { 1.0f, 1.0f };
	}

	V2_float camera_size{ GetCameraViewport(relative_to_camera).size };

	V2_float primary_camera_size{ GetCameraViewport(camera).size };

	PTGN_ASSERT(camera_size.BothAboveZero());

	V2_float scale{ primary_camera_size / camera_size };

	PTGN_ASSERT(scale.BothAboveZero());

	return scale;
}

V2_float Scene::GetRenderTargetScaleRelativeTo(Entity relative_to_camera) const {
	auto cam{ relative_to_camera ? relative_to_camera : camera };

	V2_float camera_size{ GetCameraViewport(cam).size };

	// auto camera_zoom{ cam.GetZoom() };
	// PTGN_ASSERT(camera_zoom.BothAboveZero());
	// Not accounting for camera zoom because otherwise text scaling becomes jittery.
	// camera_size /= camera_zoom;

	// TODO: Check that this is correct.
	V2_float draw_size{ render_target_.Get<RenderTarget>().GetSize() };

	PTGN_ASSERT(camera_size.BothAboveZero());

	V2_float scale{ draw_size / camera_size };

	PTGN_ASSERT(scale.BothAboveZero());

	return scale;
}

Entity Scene::CreateEntity() {
	auto entity{ manager_.CreateEntity() };
	entity.scene_ = this;
	// entity.template Add<SceneKey>(key_);
	return entity;
}

Entity Scene::CreateEntity(UUID uuid) {
	auto entity{ manager_.CreateEntity(uuid) };
	entity.scene_ = this;
	// entity.template Add<SceneKey>(key_);
	return entity;
}

Entity Scene::CreateEntity(const json& j) {
	auto entity{ manager_.CreateEntity(j) };
	entity.scene_ = this;
	// PTGN_ASSERT(entity.Has<SceneKey>(), "Scene entity created from json must have a scene key");
	return entity;
}

// TODO: Fix.
// void Scene::SetBackgroundColor(Color background_color) {
//	// render_target_.SetClearColor(background_color);
//}
// TODO: Fix.
// Color Scene::GetBackgroundColor() const {
//	// return render_target_.GetClearColor();
//	return {};
//}

Entity Scene::GetRenderTarget() const {
	return render_target_;
}

void Scene::Refresh() {
	manager_.Refresh();
}

void to_json(json& j, const Scene& scene) {
	to_json(j["manager"], scene.manager_);
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
	scene.manager_.Reset();

	// j.at("key").get_to(scene.key_);

	// Ensure manager is deserialized before any of the other scene systems which may reference
	// manager entities (such as the CameraManager).
	from_json(j.at("manager"), scene.manager_);

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

} // namespace ptgn