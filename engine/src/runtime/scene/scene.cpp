#include "runtime/scene/scene.h"

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "app/context.h"
#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "renderer/backend/gl/gl_renderer.h"
#include "renderer/camera/camera.h"
#include "renderer/camera/viewport.h"
#include "renderer/renderer.h"
#include "renderer/resources/render_target.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/vertex.h"
#include "runtime/ecs/components/camera_component.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/drawable.h"
#include "runtime/ecs/components/render_target_component.h"
#include "runtime/ecs/components/shape.h"
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

	render_target_ = CreateRenderTarget(*this, ResizeMode::DisplaySize, TextureFormat::RGBA8);
	render_target_.Remove<impl::IDrawable>();
	camera		 = CreateCamera(*this);
	fixed_camera = CreateCamera(*this);
	fixed_camera.SetMasks(kLayersNone, kLayersAll);
	// PTGN_LOG("[scene=", this, "]");
	// PTGN_LOG("[rt=", render_target_, "]");
	// PTGN_LOG("[camera=", camera, "]");
	// PTGN_LOG("[fixed_camera=", fixed_camera, "]");
	Refresh();
}

void Scene::InternalEnter() {
	OnEnter();
	Refresh();
}

void Scene::InternalEmit(EventDispatcher d) {
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
	PTGN_ASSERT(entity.Has<impl::Visible>(), "Cannot render entity without visible component");

	const auto& drawable{ entity.Get<impl::IDrawable>() };

	const auto& drawable_functions{ impl::IDrawable::data() };

	PTGN_ASSERT(drawable_functions.contains(drawable.hash), "Failed to identify drawable hash");

	const auto& draw_function{ drawable_functions.find(drawable.hash)->second };

	draw_function(renderer, entity);
}

void Scene::InternalDraw() {
	for (auto [e, _camera] : EntitiesWith<impl::CameraData>()) {
		impl::RecalculateCameraViewProjection(Camera{ e });
	}

	auto& renderer{ app().renderer };

	// { Hash(render_target), camera }
	std::unordered_map<std::size_t, std::vector<Entity>> rt_to_cameras;

	std::size_t scene_render_target{ Hash(render_target_) };

	for (auto [e, _camera] : EntitiesWith<impl::CameraData>()) {
		if (auto parent{ e.TryGet<impl::ParentRenderTarget>() }) {
			rt_to_cameras[parent->render_target].emplace_back(e);
		} else {
			rt_to_cameras[scene_render_target].emplace_back(e);
		}
	}

	std::vector<Entity> drawables;
	drawables.reserve(25);

	// impl::KDTree tree{ 20 };
	// std::vector<impl::KDObject> objects;

	for (auto [e, _visible, _drawable] : EntitiesWith<impl::Visible, impl::IDrawable>()) {
		drawables.push_back(e);

		// if (auto shape = GetSpriteOrShape(e)) {
		//	auto transform{ GetWorldTransform(e) };
		//	objects.emplace_back(e, GetBoundingAABB(*shape, transform));
		// }
	}

	SortByDepth(drawables);

	std::vector<Entity> render_targets;

	for (auto [e, _rt, _visible, _drawable] :
		 EntitiesWith<impl::RenderTargetObject, impl::Visible, impl::IDrawable>()) {
		PTGN_ASSERT(Hash(e) != scene_render_target);
		render_targets.emplace_back(e);
	}

	SortByDepth(render_targets);

	auto game_size{ renderer.GetGameSize() };

	auto draw_to_render_target = [&](RenderTarget render_target) {
		render_target.Bind();

		// TODO: Bind guard outside this loop to avoid redundant binds if multiple render targets
		// exist.
		// TODO: Fix. Clear render target with its clear color instead of transparent.
		render_target.Clear(color::Transparent);

		auto it = rt_to_cameras.find(Hash(render_target));
		if (it == rt_to_cameras.end()) {
			return;
		}

		auto rt_size{ render_target.GetSize() };

		auto scale{ V2_float{ rt_size } / game_size };

		auto& cameras{ it->second };

		SortByDepth(cameras);

		for (const auto& c : cameras) {
			Camera cam{ c };
			auto viewport{ cam.GetViewport() };
			viewport.position = viewport.position * scale;
			viewport.size	  = viewport.size * scale;
			renderer.SetViewport(viewport);
			renderer.SetViewProjection(cam.GetViewProjection());

			// auto vertices{ GetCameraWorldVertices(cam) };
			//  auto frustum_objects{ tree.Query(BoundingAABB{ vertices[0], vertices[2] }) };

			for (const auto& drawable : drawables) {
				// Mask test (entity layers vs camera include/exclude)
				if (!cam.IsVisible(drawable)) {
					continue;
				}

				// Frustum culling
				/*if (!VectorContains(frustum_objects, drawable)) {
					continue;
				}*/

				InvokeDrawable(renderer, drawable);
			}
		}
	};

	for (const auto& render_target : render_targets) {
		draw_to_render_target(RenderTarget{ render_target });
	}

	draw_to_render_target(render_target_);

	// TODO: Fix.
	/*
	if (collider_visibility_) {
		for (auto [entity, collider] : EntitiesWith<Collider>()) {
			Application::Get().debug_.DrawShape(
				GetDrawTransform(entity), collider.shape, collider_color_, collider_line_width_,
				GetDrawOrigin(entity), entity.GetCamera()
			);
		}
	}
	*/

	Viewport viewport{ {}, renderer.GetDisplayViewport().size };
	auto half_viewport{ viewport.size * 0.5f };

	renderer.BindScreenTarget();
	renderer.SetViewport(viewport);
	renderer.SetViewProjection(Matrix4::Orthographic(-half_viewport, half_viewport));
	renderer.SetBlend(BlendMode::Blend);

	auto transform{ GetDrawTransform(render_target_) };
	auto scene_target_size{ render_target_.GetSize() };
	auto positions{ Rect{ scene_target_size }.GetWorldVertices(transform, Origin::Center) };

	renderer.DrawQuadTexture(render_target_, positions, GetTint(render_target_), 0.0f, true, {});
}

void Scene::InternalUpdate() {
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

Entity Scene::CreateEntity() {
	auto entity{ manager_.CreateEntity() };
	entity.scene_ = this;
	return entity;
}

Entity Scene::CreateEntity(UUID uuid) {
	auto entity{ manager_.CreateEntity(uuid) };
	entity.scene_ = this;
	return entity;
}

Entity Scene::CreateEntity(const json& j) {
	auto entity{ manager_.CreateEntity(j) };
	entity.scene_ = this;
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