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
#include "core/event/event_dispatcher.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "core/util/concepts.h"
#include "core/util/span.h"
#include "ecs/ecs.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture_format.h"
#include "renderer/vertex/vertex.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/manager.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/render_target.h"
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

LocalSceneManager::LocalSceneManager(SceneManager& scene_manager, Scene& scene) :
	scene_manager_{ scene_manager }, scene_{ scene } {}

bool LocalSceneManager::CanIssueCommands(std::size_t target_key) const {
	if (scene_.IsTransitioning()) {
		return false;
	}

	if (!scene_manager_.HasScene(target_key)) {
		return true;
	}

	if (const auto& target_scene{ scene_manager_.GetScene(target_key) };
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
	input{ parent_scene, app.window_ },
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

void Scene::InternalEmit() {
	auto& events{ ctx().event };

	auto current = std::exchange(events.queue_, {});

	for (auto& event : current) {
		EventDispatcher dispatcher{ event };

		if (event.entity.has_value()) {
			// Single entity event.
			event.entity->OnEvent(dispatcher);
			continue;
		}

		// Global event, dispatched to all entities in the scene.
		for (auto [e, scripts] : EntitiesWith<impl::Scripts>()) {
			e.OnEvent(dispatcher);
			if (dispatcher.IsHandled()) {
				break;
			}
		}

		if (!dispatcher.IsHandled()) {
			OnEvent(dispatcher);
		}

		for (auto [e, scripts] : EntitiesWith<impl::Scripts>()) {
			scripts.ApplyPending();
		}
	}
}

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

static void SortDrawCommands(std::vector<impl::DrawCommand>& draw_commands) {
	std::ranges::stable_sort(
		draw_commands,
		[&](const impl::DrawCommand& a, const impl::DrawCommand& b) {
			const bool a_is_entity = std::holds_alternative<Entity>(a.payload);
			const bool b_is_entity = std::holds_alternative<Entity>(b.payload);

			// If only one is an Entity, it comes first
			if (a_is_entity != b_is_entity) {
				return !a_is_entity; // false -> a comes first
			}

			// For consecutive entity draw commands with the same depth, we sort them by
			// reverse creation order (logic explained below).
			if (a_is_entity && b_is_entity && a.depth == b.depth) {
				return !std::get<Entity>(a.payload).WasCreatedBefore(std::get<Entity>(b.payload));
			}

			// Sorting in reverse depth order so that we can iterate backwards and
			// prioritize entities drawing before manual draw commands.
			return a.depth >= b.depth;
		}
	);
}

struct ClearedEntities {
	std::vector<RenderTarget>& render_targets;
	std::vector<Camera>& cameras;
};

template <
	typename T, InvocableR<void, std::vector<T>&> F,
	InvocableR<void, DrawContext&, const std::vector<T>&, Camera> D>
static void DrawCommands(
	std::vector<std::pair<Camera, std::vector<T>>>& commands, DrawContext& draw_context,
	const RenderTarget& scene_render_target, ClearedEntities cleared, V2_int game_size,
	F&& sort_func, D&& draw_func
) {
	PTGN_ASSERT((!VectorContainsDuplicates(commands, [](const auto& c1, const auto& c2) {
		return c1.first == c2.first;
	})));

	impl::EntityDepthCompare compare{ true };

	std::ranges::sort(commands, [&](const auto& a, const auto& b) {
		return compare(a.first, b.first);
	});

	for (auto& [cam, cmds] : commands) {
		RenderTarget render_target;

		if (auto parent_rt = cam.template TryGet<impl::ParentRenderTarget>()) {
			render_target = parent_rt->render_target;
		} else {
			render_target = scene_render_target;
		}

		render_target.Bind();

		if (bool clear_render_target{
				!std::ranges::contains(cleared.render_targets, render_target) };
			clear_render_target) {
			render_target.Clear();
			cleared.render_targets.emplace_back(render_target);
		}

		auto rt_size{ render_target.GetSize() };
		V2_float scale{ V2_float{ rt_size } / game_size };

		auto viewport{ cam.GetViewport() };
		viewport.position = viewport.position * scale;
		viewport.size	  = viewport.size * scale;
		draw_context.SetViewport(viewport);
		draw_context.SetViewProjection(cam.GetViewProjection());

		if (bool clear_camera{ !std::ranges::contains(cleared.cameras, cam) }; clear_camera) {
			if (auto clear_color{ cam.GetClearColor() }; clear_color.has_value()) {
				draw_context.SetScissor(ScissorState{ viewport });
				render_target.Clear(*clear_color, false);
				draw_context.SetScissor(ScissorState{ false });
				cleared.cameras.emplace_back(cam);
			}
		}

		sort_func(cmds);

		draw_func(draw_context, cmds, cam);
	}
	commands.clear();
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
			/*if (!std::ranges::contains(frustum_objects, drawable)) {
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

	ctx().input.DrawDebug();

	DrawContext draw_context{ ctx().global_renderer_ };

	std::vector<Camera> cleared_cameras;
	std::vector<RenderTarget> cleared_render_targets;

	DrawCommands(
		ctx().renderer.draw_commands_, draw_context, render_target_,
		{ cleared_render_targets, cleared_cameras }, game_size, &SortDrawCommands,
		[](auto& draw_context, const auto& cmds, auto camera) {
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

	DrawCommands(
		ctx().renderer.debug_commands_, draw_context, render_target_,
		{ cleared_render_targets, cleared_cameras }, game_size,
		[](auto&) {
			/* No-op, debug commands are not sorted by depth */
		},
		[](auto& draw_context, const auto& cmds, [[maybe_unused]] auto camera) {
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

	InternalEmit();

	for (auto [e, scripts] : EntitiesWith<impl::Scripts>()) {
		scripts.Update();
	}

	OnUpdate();

	ParticleEmitter::Update(*this);
	Tween::Update(*this, ctx().dt());
	impl::AnimationSystem::Update(*this);
	Lifetime::Update(*this);
	ctx().physics.PreCollisionUpdate();
	ctx().collision.Update(*this, ctx().dt());
	ctx().physics.PostCollisionUpdate();

	Refresh();

	impl::OrphanChildren(*this);
	impl::ClearDeadChildren(*this);
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