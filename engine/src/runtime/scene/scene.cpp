#include "runtime/scene/scene.h"

#include <ecs/ecs.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/application.h"
#include "core/assert.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/util/hash.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/scaling_mode.h"
#include "renderer/pipeline/vertex.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture_format.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/manager.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/uuid.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/physics/collision_handler.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/physics.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_event_handler.h"
#include "runtime/scene/scene_transition.h"
#include "runtime/scripting/script.h"
#include "serialization/json/json.h"

namespace ptgn {

namespace {

std::vector<impl::CameraEntityCommands> GetEntityRenderCommands(
	Scene& scene, const std::optional<Camera>& primary_world_camera
) {
	std::vector<impl::CameraEntityCommands> entity_commands;

	auto get_entity_commands_for_camera =
		[&entity_commands](const auto& camera) -> impl::CameraEntityCommands& {
		if (auto it{ std::ranges::find_if(
				entity_commands,
				[&camera](const auto& camera_entity_commands) {
					return camera_entity_commands.camera == camera;
				}
			) };
			it != entity_commands.end()) {
			return *it;
		}

		return entity_commands.emplace_back(impl::CameraEntityCommands{ .camera = camera });
	};

	impl::ForDrawableSceneEntities(
		scene, primary_world_camera,
		[&get_entity_commands_for_camera](auto& scene_ref, const auto& camera, const auto& filter) {
			if (camera.scene_camera) {
				impl::RecalculateCameraViewProjection(camera.scene_camera);
			}

			auto& camera_entity_commands{ get_entity_commands_for_camera(camera) };

			for (auto [entity, _visible, _drawable] :
				 scene_ref.template EntitiesWith<impl::Visible, impl::IDrawable>()) {
				// Mask test (entity layers vs camera include/exclude).
				if (filter(entity)) {
					continue;
				}

				camera_entity_commands.commands.emplace_back(entity, GetDepth(entity));
			}
		}
	);

	return entity_commands;
}

} // namespace

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

void Scene::InternalDraw(DrawContext& draw_context) {
	const auto& primary_world_camera{ ctx().renderer.GetPrimaryWorldCamera() };

	auto entity_commands{ GetEntityRenderCommands(*this, primary_world_camera) };

	ctx().collision.DrawDebug(*this);
	ctx().interaction.DrawDebug(*this);

	impl::ClearedEntities cleared;

	auto game_size{ ctx().global_renderer_.GetGameSize() };

	auto buckets{ ctx().renderer.GetRenderBuckets(ctx().renderer.draw_commands_, entity_commands) };

	ctx().renderer.Draw(draw_context, render_target_, cleared, game_size, buckets);

	if (primary_world_camera.has_value()) {
		impl::RenderCamera render_camera{ *primary_world_camera };
		ctx().renderer.CombineDebugCommands(render_camera);

		PTGN_ASSERT(ctx().renderer.draw_commands_.size() == 1);
		PTGN_ASSERT(ctx().renderer.debug_commands_.size() == 1);
	}

	// Currently always empty.
	std::vector<impl::CameraEntityCommands> debug_entity_commands;

	auto debug_buckets{
		ctx().renderer.GetRenderBuckets(ctx().renderer.debug_commands_, debug_entity_commands)
	};

	ctx().renderer.Draw(draw_context, render_target_, cleared, game_size, debug_buckets);

	ctx().global_renderer_.FlushBatch();

	Viewport viewport{ {}, ctx().global_renderer_.GetDisplayViewport().size };

	ctx().global_renderer_.BindScreenTarget();
	ctx().global_renderer_.SetViewport(viewport);
	ctx().global_renderer_.SetViewProjection(viewport.size);
	ctx().global_renderer_.SetBlendMode(BlendMode::Blend);

	auto render_target_texture{ ctx().global_renderer_.GetRenderTargetTexture(render_target_) };
	auto draw_transform{ GetDrawTransform(render_target_) };
	auto scene_target_size{ render_target_.GetSize() };
	auto rt_tint{ GetTint(render_target_) };

	constexpr auto draw_origin{ Origin::Center };
	constexpr auto depth{ 0.0f };
	constexpr auto tex_coords{ impl::GetDefaultTextureCoordinates<true>() };
	constexpr auto entity_id{ -1 };

	auto effects{ impl::GetEffectParams(render_target_) };

	draw_context.DrawTexture(
		render_target_texture, draw_transform, depth, scene_target_size, draw_origin, rt_tint,
		tex_coords, effects, entity_id
	);

	ctx().global_renderer_.FlushBatch();
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

Entity Scene::CreateEntity(std::optional<std::string_view> tag, std::optional<std::uint64_t> uuid) {
	auto entity{ manager_.CreateEntity() };
	impl::AddMandatoryComponents(entity, tag, uuid);
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

Scene::Scene(Scene&&) noexcept = default;

Scene& Scene::operator=(Scene&&) noexcept = default;

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
	PTGN_ASSERT(ctx_, "Scene context has not been set yet");
	return *ctx_;
}

const SceneContext& Scene::ctx() const {
	PTGN_ASSERT(ctx_, "Scene context has not been set yet");
	return *ctx_;
}

} // namespace ptgn