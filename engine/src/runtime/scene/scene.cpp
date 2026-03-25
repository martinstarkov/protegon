#include "runtime/scene/scene.h"

#include <algorithm>
#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "app/application.h"
#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/util/span.h"
#include "ecs/ecs.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/render_state.h"
#include "renderer/primitives/texture_format.h"
#include "renderer/primitives/vertex.h"
#include "renderer/primitives/viewport.h"
#include "renderer/renderer.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/particle.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/render_target_component.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/collision_handler.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/physics.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_state.h"
#include "runtime/scripting/scripts.h"
#include "serialization/json/json.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

SceneEventHandler::SceneEventHandler(Scene& scene) : scene_{ scene } {}

void SceneEventHandler::Emit(EventDispatcher d) {
	scene_.InternalEmit(d);
}

LocalSceneManager::LocalSceneManager(SceneManager& scene_manager, Scene& scene) :
	scene_manager_{ scene_manager }, scene_{ scene } {}

bool LocalSceneManager::CanIssueCommands(std::size_t target_key) const {
	if (scene_.IsTransitioning()) {
		return false;
	}

	if (!scene_manager_.Has(target_key)) {
		return true;
	}

	if (const auto& target_scene{ scene_manager_.Get(target_key) };
		target_scene.IsTransitioning()) {
		return false;
	}

	return true;
}

SceneContext::SceneContext(Application& app, Scene& parent_scene) :
	global_event{ app.events_ },
	window{ app.window_ },
	asset{ app.assets_ },
	font{ app.font_ },
	audio{ app.audio_ },
	scene{ app.scenes_, parent_scene },
	renderer{ parent_scene, app.renderer_ },
	debug{ renderer },
	event{ parent_scene },
	input{ parent_scene, app.input_ },
	physics{ parent_scene },
	global_renderer_{ app.renderer_ },
	app_{ app } {}

SceneContext::~SceneContext() noexcept {
	// Needs access to destructors.
}

void SceneContext::Stop() {
	app_.Stop();
}

secondsf SceneContext::dt() const {
	return app_.dt();
}

milliseconds SceneContext::TimeSinceStart() const {
	return app_.TimeSinceStart();
}

bool SceneContext::IsRunning() const {
	return app_.IsRunning();
}

std::size_t SceneContext::GetFrameCount() const {
	return app_.GetFrameCount();
}

Scene::Scene() {}

Scene::~Scene() {}

void Scene::Init(Application& app) {
	ctx_ = std::make_unique<SceneContext>(app, *this);

	ctx_->camera		= CreateCamera(*this);
	ctx_->fixed_camera_ = CreateCamera(*this);
	ctx_->fixed_camera_.SetMasks(kLayersNone, kLayersAll);
	SetUI(ctx_->fixed_camera_, true);

	render_target_ = CreateRenderTarget(
		*this, ResizeMode::DisplaySize, color::Transparent, TextureFormat::RGBA8
	);
	render_target_.Remove<impl::IDrawable>();

	// PTGN_LOG("[scene=", this, "]");
	// PTGN_LOG("[rt=", render_target_, "]");
	// PTGN_LOG("[camera=", camera, "]");
	// PTGN_LOG("[fixed_camera=", fixed_camera, "]");
	Refresh();

	for (auto [e, scripts] : EntitiesWith<impl::Scripts>()) {
		scripts.ApplyPending();
	}

	Refresh();
}

void Scene::InternalEnter() {
	OnEnter();
	Refresh();

	for (auto [e, scripts] : EntitiesWith<impl::Scripts>()) {
		scripts.ApplyPending();
	}

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

bool Scene::IsTransitioning() const {
	return state_ == impl::SceneState::TransitionIn || state_ == impl::SceneState::TransitionOut;
}

static void InvokeDrawable(DrawContext& draw_context, Entity entity, Camera camera) {
	PTGN_ASSERT(entity.Has<impl::IDrawable>(), "Cannot render entity without drawable component");
	PTGN_ASSERT(entity.Has<impl::Visible>(), "Cannot render entity without visible component");

	const auto& drawable{ entity.Get<impl::IDrawable>() };

	const auto& drawable_functions{ impl::IDrawable::data() };

	PTGN_ASSERT(drawable_functions.contains(drawable.hash), "Failed to identify drawable hash");

	const auto& draw_function{ drawable_functions.find(drawable.hash)->second };

	draw_function(draw_context, entity, camera);
}

void Scene::InternalDraw() {
	auto game_size{ ctx().renderer.GetGameSize() };

	for (auto [c, _camera] : EntitiesWith<impl::CameraData>()) {
		Camera cam{ c };
		impl::RecalculateCameraViewProjection(Camera{ cam });

		for (auto entity : Entities()) {
			bool visible{ entity.Has<impl::Visible, impl::IDrawable>() };

			if (!ctx().collision.settings_.debug_draw_enabled && !visible) {
				continue;
			}

			// Mask test (entity layers vs camera include/exclude).
			if (!cam.IsVisible(entity)) {
				continue;
			}

			// Frustum culling.
			/*if (!VectorContains(frustum_objects, drawable)) {
				continue;
			}*/

			if (visible) {
				auto& draw_commands{ ctx().renderer.GetDrawCommandsForCamera(cam) };
				draw_commands.emplace_back(entity, GetDepth(entity));
			}

			if (ctx().collision.settings_.debug_draw_enabled && entity.Has<Collider>()) {
				const auto& collider{ entity.Get<Collider>() };
				auto transform{ GetDrawTransform(entity) };
				auto draw_origin{ GetDrawOrigin(entity) };
				ctx().debug.DrawShape(
					collider.shape, transform, ctx().collision.settings_.debug_draw_color,
					ctx().collision.settings_.debug_draw_fill_style, draw_origin, cam
				);
			}
		}
	}

	impl::EntityDepthCompare compare{ true };

	DrawContext draw_context{ ctx().global_renderer_ };

	std::vector<Camera> cleared_cameras;
	std::vector<RenderTarget> cleared_render_targets;

	const auto draw_commands = [&](auto& commands, const auto& sort_func, const auto& draw_func) {
		PTGN_ASSERT((!VectorContainsDuplicates(commands, [](const auto& c1, const auto& c2) {
			return c1.first == c2.first;
		})));

		std::ranges::sort(commands, [&](const auto& a, const auto& b) {
			return compare(a.first, b.first);
		});

		for (auto& [cam, cmds] : commands) {
			RenderTarget render_target;

			if (auto parent_rt = cam.TryGet<impl::ParentRenderTarget>()) {
				render_target = parent_rt->render_target;
			} else {
				render_target = render_target_;
			}

			render_target.Bind();

			bool clear_render_target{ !VectorContains(cleared_render_targets, render_target) };

			if (clear_render_target) {
				render_target.Clear();
				cleared_render_targets.emplace_back(render_target);
			}

			auto rt_size{ render_target.GetSize() };
			V2_float scale{ V2_float{ rt_size } / game_size };

			auto viewport{ cam.GetViewport() };
			viewport.position = viewport.position * scale;
			viewport.size	  = viewport.size * scale;
			draw_context.SetViewport(viewport);
			draw_context.SetViewProjection(cam.GetViewProjection());

			bool clear_camera{ !VectorContains(cleared_cameras, cam) };

			if (clear_camera) {
				if (auto clear_color{ cam.GetClearColor() }; clear_color.has_value()) {
					draw_context.SetScissor(ScissorState{ viewport });
					render_target.Clear(*clear_color, false);
					draw_context.SetScissor(ScissorState{ false });
					cleared_cameras.emplace_back(cam);
				}
			}

			sort_func(cmds);

			draw_func(cmds, cam);
		}
		commands.clear();
	};

	draw_commands(
		ctx().renderer.draw_commands_,
		[](auto& cmds) {
			std::ranges::stable_sort(
				cmds,
				[&](const impl::DrawCommand& a, const impl::DrawCommand& b) {
					// For consecutive entity draw commands with the same depth, we sort them by
					// reverse creation order (logic explained below).
					if (a.depth == b.depth && std::holds_alternative<Entity>(a.payload) &&
						std::holds_alternative<Entity>(b.payload)) {
						return !std::get<Entity>(a.payload).WasCreatedBefore(
							std::get<Entity>(b.payload)
						);
					}

					// Sorting in reverse depth order so that we can iterate backwards and
					// prioritize entities drawing before manual draw commands.
					return a.depth >= b.depth;
				}
			);
		},
		[&draw_context](const auto& cmds, auto camera) {
			for (std::size_t i{ 0 }; i < cmds.size(); i++) {
				// By iterating backwards, we ensure that the most recently added commands are drawn
				// last. This prioritizes drawing entities first followed by manual draw commands.
				const auto& draw_cmd{ cmds[cmds.size() - 1 - i] };
				if (std::holds_alternative<Entity>(draw_cmd.payload)) {
					InvokeDrawable(draw_context, std::get<Entity>(draw_cmd.payload), camera);
				} else {
					draw_context.Draw(
						std::get<impl::ManualCommand>(draw_cmd.payload), draw_cmd.depth
					);
				}
			}
		}
	);

	draw_commands(
		ctx().renderer.debug_commands_,
		[](auto&) {
			/* No-op, debug commands are not sorted by depth */
		},
		[&draw_context](const auto& cmds, [[maybe_unused]] auto camera) {
			for (const auto& draw_cmd : cmds) {
				draw_context.Draw(draw_cmd.payload, draw_cmd.depth);
			}
		}
	);

	ctx().global_renderer_.FlushBatch();

	Viewport viewport{ {}, ctx().renderer.GetDisplayViewport().size };
	V2_float half_viewport{ viewport.size * 0.5f };

	auto view_projection{ Matrix4::Orthographic(-half_viewport, half_viewport) };
	draw_context.BindScreenTarget();
	draw_context.SetViewport(viewport);
	draw_context.SetViewProjection(view_projection);
	draw_context.SetBlend(BlendMode::Blend);

	auto transform{ GetDrawTransform(render_target_) };
	auto scene_target_size{ render_target_.GetSize() };
	Rect scene_rect{ V2_float{ scene_target_size } };
	auto positions{ scene_rect.GetWorldVertices(transform, Origin::Center) };

	auto tex_coords{ impl::GetDefaultTextureCoordinates<true>() };
	auto rt_tint{ GetTint(render_target_) };

	auto quad_shader{ ctx().global_renderer_.GetShader("quad") };

	auto render_target_texture{ ctx().global_renderer_.GetRenderTargetTexture(render_target_) };

	ctx().global_renderer_.DrawTexture(
		quad_shader, render_target_texture, positions, rt_tint, 0.0f, tex_coords, {}
	);

	ctx().global_renderer_.FlushBatch();

	// Must be cleared after BindScreenTarget, as that flushes the batch.
	ctx().renderer.temporary_textures_.clear();
}

void Scene::InternalUpdate() {
	ctx().input.Update();

	Refresh();

	for (auto [e, scripts] : EntitiesWith<impl::Scripts>()) {
		scripts.Update();
	}

	OnUpdate();
	Refresh();

	ParticleEmitter::Update(*this);
	Tween::Update(*this, ctx().dt());
	impl::AnimationSystem::Update(*this);
	Lifetime::Update(*this);
	ctx().physics.PreCollisionUpdate();
	ctx().collision.Update(*this);
	ctx().physics.PostCollisionUpdate();
}

void Scene::InternalExit() {
	Refresh();
	OnExit();
	Refresh();
	// Clears component hooks.
	manager_.Reset();
	ctx().physics.Reset();
	Refresh();
}

// TODO: Fix.
// void Scene::ReEnter() {
//	app().scene.Enter(*this);
// }

Entity Scene::GetEntityByUUID(UUID uuid) const {
	auto entities{ Entities() };
	for (Entity e : entities) {
		PTGN_ASSERT(e.Has<UUID>(), "Entity does not have a valid UUID component");
		if (e.Get<UUID>() == uuid) {
			return e;
		}
	}
	return {};
}

Entity Scene::CreateEntity() {
	return CreateEntity(UUID{});
}

Entity Scene::CreateEntity(UUID uuid) {
	auto entity{ manager_.CreateEntity() };
	entity.Add<UUID>(uuid);
	return Entity{ entity, this };
}

Entity Scene::CreateEntity(const json& j) {
	auto entity{ manager_.CreateEntity() };
	PTGN_ASSERT(entity, "Failed to create entity");
	Entity e{ entity, this };
	e.Deserialize(j);
	PTGN_ASSERT(e.Has<UUID>(), "Entity created from json must have a UUID");
	return e;
}

void Scene::SetBackgroundColor(Color background_color) {
	render_target_.SetClearColor(background_color);
}

Color Scene::GetBackgroundColor() const {
	return render_target_.GetClearColor();
}

RenderTarget Scene::GetRenderTarget() const {
	return render_target_;
}

void Scene::Refresh() {
	manager_.Refresh();
}

std::size_t Scene::GetEntityCount() const {
	return manager_.Size();
}

void to_json(json& j, const Scene& scene) {
	to_json(j["manager"], scene.manager_);
	/*
	j["key"]				 = scene.key_;
	j["physics"]			 = scene.physics;
	j["input"]				 = scene.input;
	j["collider_visibility"] = scene.collider_visibility_;
	j["collider_color"]		 = scene.collider_color_;
	*/
}

void from_json(const json& j, Scene& scene) {
	scene.manager_.Reset();

	// Ensure manager is deserialized before any of the other scene systems which may reference
	// manager entities (such as the CameraManager).
	from_json(j.at("manager"), scene.manager_);

	// j.at("physics").get_to(scene.physics);
	// j.at("input").get_to(scene.input);
}

SceneContext& Scene::ctx() {
	PTGN_ASSERT(ctx_ != nullptr, "Scene context has not been set yet");
	return *ctx_;
}

const SceneContext& Scene::ctx() const {
	PTGN_ASSERT(ctx_ != nullptr, "Scene context has not been set yet");
	return *ctx_;
}

} // namespace ptgn