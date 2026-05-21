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
#include "renderer/resources/id.h"
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
#include "runtime/scene/scene_transition.h"
#include "runtime/scripting/script.h"
#include "serialization/json/json.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

namespace {

void InvokeDrawable(DrawContext& draw_context, Entity entity) {
	PTGN_ASSERT(entity.Has<impl::IDrawable>(), "Cannot render entity without drawable component");
	PTGN_ASSERT(entity.Has<impl::Visible>(), "Cannot render entity without visible component");

	const auto& drawable{ entity.Get<impl::IDrawable>() };

	const auto& drawable_functions{ impl::IDrawable::data() };

	PTGN_ASSERT(drawable_functions.contains(drawable.hash), "Failed to identify drawable hash");

	const auto& draw_function{ drawable_functions.find(drawable.hash)->second };

	draw_function(draw_context, entity);
}

void SortDrawCommands(std::vector<impl::DrawCommand>& draw_commands) {
	std::ranges::stable_sort(
		draw_commands, [&](const impl::DrawCommand& a, const impl::DrawCommand& b) {
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
	std::vector<std::size_t>& cameras;
};

template <
	typename T, InvocableR<void, std::vector<T>&> F,
	InvocableR<void, DrawContext&, const std::vector<T>&> D>
void DrawCommands(
	std::vector<std::pair<impl::RenderCamera, std::vector<T>>>& commands, impl::Renderer& renderer,
	DrawContext& draw_context, const RenderTarget& scene_render_target, ClearedEntities cleared,
	V2_int game_size, F&& sort_func, D&& draw_func
) {
	PTGN_ASSERT((!ContainsDuplicates(commands, [](const auto& c1, const auto& c2) {
		return c1.first == c2.first;
	})));

	std::ranges::stable_sort(commands, [&](const auto& a, const auto& b) {
		// TODO: Incorporate creation order of cameras.
		return a.first.depth < b.first.depth;
	});

	for (auto& [cam, cmds] : commands) {
		RenderTarget render_target{ cam.render_target.value_or(scene_render_target) };

		render_target.Bind();

		if (bool clear_render_target{
				!std::ranges::contains(cleared.render_targets, render_target) };
			clear_render_target) {
			render_target.Clear();
			cleared.render_targets.emplace_back(render_target);
		}

		auto rt_size{ render_target.GetSize() };
		V2_float scale{ V2_float{ rt_size } / game_size };

		auto viewport{ cam.camera.viewport };
		viewport.position = viewport.position * scale;
		viewport.size	  = viewport.size * scale;
		renderer.SetViewport(viewport);
		renderer.SetViewProjection(cam.camera.view_projection);

		if (bool clear_camera{ !std::ranges::contains(cleared.cameras, cam.uuid) }; clear_camera) {
			if (cam.clear_color.has_value()) {
				renderer.SetScissor(ScissorState{ viewport });
				render_target.Clear(*cam.clear_color, false);
				renderer.SetScissor(ScissorState{ false });
				cleared.cameras.emplace_back(cam.uuid);
			}
		}

		sort_func(cmds);

		draw_func(draw_context, cmds);
	}
	commands.clear();
}

void CombineDebugCommands(
	std::vector<std::pair<impl::RenderCamera, std::vector<impl::ManualCommand>>>& debug_commands,
	const impl::RenderCamera& camera
) {
	std::vector<impl::ManualCommand> combined;

	std::size_t total = 0;
	for (auto& [_, cmds] : debug_commands) {
		total += cmds.size();
	}

	combined.reserve(total);

	for (auto& [_, cmds] : debug_commands) {
		combined.insert(
			combined.end(), std::make_move_iterator(cmds.begin()),
			std::make_move_iterator(cmds.end())
		);
	}

	debug_commands.clear();
	debug_commands.emplace_back(camera, std::move(combined));
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

void Scene::InvokeEntityDrawCommands(
	Scene& scene, const impl::RenderCamera& render_camera, const std::function<bool(Entity)>& filter
) {
	const auto& collision_debug{ scene.ctx().collision.GetDebugSettings() };

	for (auto entity : scene.Entities()) {
		bool visible{ entity.Has<impl::Visible, impl::IDrawable>() };

		if (!collision_debug.draw_enabled && !visible) {
			continue;
		}

		// Mask test (entity layers vs camera include/exclude).
		if (filter(entity)) {
			continue;
		}

		// Frustum culling.
		/*if (!std::ranges::contains(frustum_objects, drawable)) {
			continue;
		}*/

		if (visible) {
			auto& draw_commands{ scene.ctx().renderer.GetDrawCommandsForCamera(render_camera) };
			draw_commands.emplace_back(entity, GetDepth(entity));
		}

		if (collision_debug.draw_enabled && entity.Has<Collider>()) {
			const auto& collider{ entity.Get<Collider>() };
			auto transform{ GetDrawTransform(entity) };
			auto draw_origin{ GetDrawOrigin(entity) };
			scene.ctx().debug.DrawShape(
				collider.shape, transform, collision_debug.draw_color,
				collision_debug.draw_fill_style, draw_origin, render_camera
			);
		}
	}
}

void Scene::InternalDraw(DrawContext& draw_context) {
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

	std::vector<std::size_t> cleared_cameras;
	std::vector<RenderTarget> cleared_render_targets;

	if (primary_world_camera.has_value()) {
		PTGN_ASSERT(ctx().renderer.draw_commands_.size() == 1);
	}

	DrawCommands(
		ctx().renderer.draw_commands_, ctx().global_renderer_, draw_context, render_target_,
		{ cleared_render_targets, cleared_cameras }, game_size, &SortDrawCommands,
		[](auto& draw_context, const auto& cmds) {
			for (auto i{ 0uz }; i < cmds.size(); ++i) {
				// By iterating backwards, we ensure that the most recently added commands are drawn
				// last. This prioritizes drawing entities first followed by manual draw commands.
				const auto& draw_cmd{ cmds[cmds.size() - 1 - i] };
				if (std::holds_alternative<Entity>(draw_cmd.payload)) {
					InvokeDrawable(draw_context, std::get<Entity>(draw_cmd.payload));
				} else {
					draw_context.Draw(std::get<impl::ManualCommand>(draw_cmd.payload));
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
		ctx().renderer.debug_commands_, ctx().global_renderer_, draw_context, render_target_,
		{ cleared_render_targets, cleared_cameras }, game_size,
		[](auto&) {
			/* No-op, debug commands are not sorted by depth */
		},
		[](auto& draw_context, const auto& cmds) {
			for (const auto& draw_cmd : cmds) {
				draw_context.Draw(draw_cmd);
			}
		}
	);

	ctx().global_renderer_.FlushBatch();

	Viewport viewport{ {}, ctx().global_renderer_.GetDisplayViewport().size };
	V2_float half_viewport{ viewport.size * 0.5f };

	auto view_projection{ Matrix4::Orthographic(-half_viewport, half_viewport) };
	ctx().global_renderer_.BindScreenTarget();
	ctx().global_renderer_.SetViewport(viewport);
	ctx().global_renderer_.SetViewProjection(view_projection);
	ctx().global_renderer_.SetBlendMode(BlendMode::Blend);

	auto transform{ GetDrawTransform(render_target_) };
	auto scene_target_size{ render_target_.GetSize() };
	Rect scene_rect{ V2_float{ scene_target_size } };
	auto positions{ scene_rect.GetWorldVertices(transform, Origin::Center) };

	auto tex_coords{ impl::GetDefaultTextureCoordinates<true>() };
	auto rt_tint{ GetTint(render_target_) };

	auto texture_shader{ ctx().global_renderer_.GetShader("texture") };

	auto render_target_texture{ ctx().global_renderer_.GetRenderTargetTexture(render_target_) };

	ctx().global_renderer_.SetShader(texture_shader);

	impl::EffectParams effects;

	draw_context.DrawTexture(
		render_target_texture, positions, 0.0f, rt_tint, tex_coords, effects, {}, -1
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