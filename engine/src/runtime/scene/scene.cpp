#include "runtime/scene/scene.h"

#include <ecs/ecs.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "app/application.h"
#include "core/assert.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/hash.h"
#include "core/util/span.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture_format.h"
#include "renderer/vertex/vertex.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/manager.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/uuid.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/collision_handler.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/physics.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event_handler.h"
#include "runtime/scene/scene_render_graph.h"
#include "runtime/scene/scene_transition.h"
#include "runtime/scripting/script.h"
#include "serialization/json/json.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

void Scene::InternalOnEvent(Event event) {
	// Global event, dispatched to all entities in the scene.
	for (auto [entity, scripts] : EntitiesWith<impl::Scripts>()) {
		entity.OnEvent(event);
		if (event.IsHandled()) {
			break;
		}
	}

	if (!event.IsHandled()) {
		OnEvent(event);
	}

	for (auto [entity, scripts] : EntitiesWith<impl::Scripts>()) {
		scripts.ApplyPending();
	}
}

void Scene::InternalOnEvent() {
	auto& events{ ctx().event };

	auto current = std::exchange(events.entity_event_queue_, {});

	for (auto& entity_event : current) {
		Event event{ entity_event.event };

		if (entity_event.entity) {
			// Single entity event.
			entity_event.entity.OnEvent(event);
			continue;
		}

		InternalOnEvent(event);
	}
}

void Scene::InternalPreUpdate() {
	ctx().interaction.Update(*this);
}

void Scene::Init(Application& app, impl::SceneData&& scene_data) {
	data_ = std::move(scene_data);
	ctx_  = std::make_unique<SceneContext>(app, *this);

	ctx_->camera = CreateCamera(*this);
	ctx_->camera.SetTag("Main Camera");
	ctx_->fixed_camera_ = CreateCamera(*this);
	ctx_->fixed_camera_.SetTag("Fixed Camera");
	ctx_->fixed_camera_.SetMasks(kLayersNone, kLayersAll);
	SetUI(ctx_->fixed_camera_, true);

	render_target_ =
		CreateRenderTarget(*this, ResizeType::Display, color::Transparent, TextureFormat::RGBA8);
	render_target_.SetTag("Scene Target");
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
	return data_.state == impl::SceneState::TransitionIn ||
		   data_.state == impl::SceneState::TransitionOut;
}

bool Scene::IsAwaitingTransitionDelay() const {
	return data_.transition && !data_.transition->IsStarted();
}

void Scene::InternalDraw() {
	auto screen{ ctx().global_renderer_.GetScreenTarget() };

	std::vector<Entity> scene_attached_effects;
	std::vector<Entity> screen_effects;

	impl::RenderSceneWithGraph(
		ctx().global_renderer_, *this, render_target_, render_target_.GetSize(),
		render_target_.GetFormat(), screen, ctx().global_renderer_.GetRenderTargetSize(screen),
		scene_attached_effects, screen_effects
	);

	/*
	// TODO: Move this logic elsewhere.

	auto game_size{ ctx().global_renderer_.GetGameSize() };

	const auto& primary_world_camera{ ctx().global_renderer_.GetPrimaryWorldCamera() };

	impl::RenderCamera render_camera;

	if (primary_world_camera.has_value()) {
		render_camera = impl::RenderCamera{ *primary_world_camera };
		// If a primary world camera is set, we draw all entities in a single pass using that
		// camera.
		InvokeEntityDrawCommands(*this, render_camera, [](auto) { return false; });
	} else {
		for (auto [c, _camera] : EntitiesWith<impl::CameraData>()) {
			SceneCamera camera{ c };
			impl::RecalculateCameraViewProjection(camera);
			InvokeEntityDrawCommands(*this, impl::RenderCamera{ camera }, [camera](auto entity) {
				return !camera.IsVisible(entity);
			});
		}
	}

	ctx().interaction.DrawDebug(*this);

	DrawContext draw_context{ ctx().global_renderer_ };

	std::vector<std::size_t> cleared_cameras;
	std::vector<RenderTarget> cleared_render_targets;

	if (primary_world_camera.has_value()) {
		PTGN_ASSERT(ctx().renderer.draw_commands_.size() == 1);
	}

	DrawCommands(
		ctx().renderer.draw_commands_, draw_context, render_target_,
		{ cleared_render_targets, cleared_cameras }, game_size, &SortDrawCommands,
		[](auto& draw_context, const auto& cmds) {
			for (std::size_t i{ 0 }; i < cmds.size(); i++) {
				// By iterating backwards, we ensure that the most recently added commands are drawn
				// last. This prioritizes drawing entities first followed by manual draw commands.
				const auto& draw_cmd{ cmds[cmds.size() - 1 - i] };
				if (std::holds_alternative<Entity>(draw_cmd.payload)) {
					InvokeDrawable(draw_context, std::get<Entity>(draw_cmd.payload));
				} else {
					draw_context.Draw(
						std::get<impl::ManualCommand>(draw_cmd.payload), draw_cmd.depth
					);
				}
			}
		}
	);

	if (primary_world_camera.has_value()) {
		// If a primary world camera is set, we combine all debug commands into a single command
		// list for that camera.
		CombineDebugCommands(ctx().renderer.debug_commands_, render_camera);
		PTGN_ASSERT(ctx().renderer.debug_commands_.size() == 1);
	}

	DrawCommands(
		ctx().renderer.debug_commands_, draw_context, render_target_,
		{ cleared_render_targets, cleared_cameras }, game_size,
		[](auto&) {
			// No-op, debug commands are not sorted by depth
		},
		[](auto& draw_context, const auto& cmds) {
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
	draw_context.SetBlendMode(BlendMode::Blend);

	auto transform{ GetDrawTransform(render_target_) };
	auto scene_target_size{ render_target_.GetSize() };
	Rect scene_rect{ V2_float{ scene_target_size } };
	auto positions{ scene_rect.GetWorldVertices(transform, Origin::Center) };

	auto tex_coords{ impl::GetDefaultTextureCoordinates<true>() };
	auto rt_tint{ GetTint(render_target_) };

	auto texture_shader{ ctx().global_renderer_.GetShader("texture") };

	auto render_target_texture{ ctx().global_renderer_.GetRenderTargetTexture(render_target_) };

	ctx().global_renderer_.DrawTexture(
		texture_shader, render_target_texture, positions, 0.0f, rt_tint, tex_coords, {}, -1
	);

	ctx().global_renderer_.FlushBatch();

	// Must be cleared after BindScreenTarget, as that flushes the batch.
	ctx().renderer.temporary_textures_.clear();
	*/
}

void Scene::InternalUpdate() {
	for (auto [e, scripts] : EntitiesWith<impl::Scripts>()) {
		scripts.Update();
	}

	OnUpdate();

	auto dt{ ctx().dt() };

	ParticleEmitter::Update(*this, dt);
	Tween::Update(*this, dt);
	impl::AnimationSystem::Update(*this, dt);
	Lifetime::Update(*this, dt);
	ctx().physics.PreCollisionUpdate();
	ctx().collision.Update(*this, dt);
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

Entity Scene::GetEntityByUUID(std::uint64_t uuid) const {
	for (const Entity& e : Entities()) {
		PTGN_ASSERT(e.Has<impl::UUID>(), "Entity does not have a valid UUID component");
		if (e.Get<impl::UUID>() == uuid) {
			return e;
		}
	}
	return {};
}

Entity Scene::GetEntityByTag(std::string_view tag) const {
	for (const Entity& e : Entities()) {
		PTGN_ASSERT(e.Has<impl::Tag>(), "Entity does not have a valid Tag component");
		if (e.Get<impl::Tag>() == tag) {
			return e;
		}
	}
	return {};
}

void Scene::AddMandatoryComponents(
	Entity entity, std::optional<std::string_view> tag, std::optional<std::uint64_t> uuid
) {
	entity.Add<impl::Tag>(tag.value_or(impl::kDefaultTag));
	entity.Add<impl::UUID>(uuid.value_or(impl::UUID{}));
}

Entity Scene::CreateEntity(std::optional<std::string_view> tag, std::optional<std::uint64_t> uuid) {
	auto entity{ manager_.CreateEntity() };
	AddMandatoryComponents(entity, tag, uuid);
	return Entity{ entity, this };
}

Entity Scene::CreateEntity(const json& j) {
	auto entity{ manager_.CreateEntity() };
	PTGN_ASSERT(entity, "Failed to create entity");
	Entity e{ entity, this };
	j.get_to(e);
	e.Deserialize(j);
	PTGN_ASSERT(e.Has<impl::UUID>(), "Entity created from json must have a UUID component");
	PTGN_ASSERT(e.Has<impl::Tag>(), "Entity created from json must have a Tag component");
	return e;
}

Scene::~Scene() = default;

void Scene::SetBackgroundColor(Color background_color) {
	render_target_.SetClearColor(background_color);
}

Color Scene::GetBackgroundColor() const {
	return render_target_.GetClearColor();
}

std::size_t Scene::GetTagHash() const {
	return data_.tag_hash;
}

std::string Scene::GetTag() const {
	return data_.tag;
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
	j["tag"] = scene.data_.tag;
}

void from_json(const json& j, Scene& scene) {
	scene.manager_.Reset();

	// Ensure manager is deserialized before any of the other scene systems which may reference
	// manager entities (such as the CameraManager).
	from_json(j.at("manager"), scene.manager_);

	j.at("tag").get_to(scene.data_.tag);
	scene.data_.tag_hash = Hash(scene.data_.tag);
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