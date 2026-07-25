#include "runtime/scene/scene.h"

#include <ecs/ecs.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "app/application.h"
#include "app/application_context.h"
#include "core/assert.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/hash.h"
#include "renderer/draw_context.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/renderer.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/entity_serialization.h"
#include "runtime/ecs/manager.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/uuid.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/render_queue.h"
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
#include "runtime/ui/button.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"
#include "tools/debug/debug_system.h"

namespace ptgn {

namespace {

constexpr TextureFormat kDefaultSceneTargetFormat{ kDefaultRenderTargetFormat };
constexpr Color kDefaultSceneBackgroundColor{ kDefaultRenderTargetClearColor };

constexpr BlendMode kDefaultFirstSceneBlendMode{ BlendMode::ReplaceRGBA };

constexpr std::string_view kDefaultSceneTargetTag{ "Scene Target" };
constexpr std::string_view kDefaultSceneCameraTag{ "Main Camera" };
constexpr std::string_view kDefaultSceneFixedCameraTag{ "Fixed Camera" };

constexpr LayerMask kDefaultFixedCameraIncludeLayerMask{ kLayersNone };
constexpr LayerMask kDefaultFixedCameraExcludeLayerMask{ kLayersAll };

void UpdateRenderTargetSizes(Scene& scene) {
	auto display_size{ scene.ctx().renderer.GetDisplaySize() };

	if (!display_size.IsPositive()) {
		display_size = scene.ctx().renderer.GetPresentationSize();
	}

	if (!display_size.IsPositive()) {
		return;
	}

	for (auto [entity, framebuffer] : scene.EntitiesWith<impl::FramebufferObject>()) {
		if (!entity.Has<impl::RenderTargetSize>()) {
			// Backwards compatibility for render targets serialized before
			// RenderTargetSize existed. Existing targets become fixed-size.
			entity.Add<impl::RenderTargetSize>(
				impl::RenderTargetSize{
					.follow_display_size = false,
					.size = framebuffer.GetDesc().size,
				}
			);
		}

		RenderTarget{ entity }.UpdateSize(display_size);
	}
}

void ApplyCameraEffects(
	Renderer& render, DrawContext& draw_context, Viewport display_viewport,
	V2_float render_target_size, const Matrix4& view_projection, Color tint,
	const impl::EffectParams& effect_params
) {
	if (tint == color::White && !effect_params.draw_callback) {
		return;
	}

	impl::RendererAccessor renderer{ render };

	auto texture{ renderer.GetTexture(renderer.GetBoundFramebuffer()) };

	PTGN_ASSERT(view_projection == draw_context.GetRenderState().view_projection);
	PTGN_ASSERT(display_viewport == draw_context.GetRenderState().viewport);
	PTGN_ASSERT(
		display_viewport == draw_context.GetRenderState().scissor.viewport &&
		draw_context.GetRenderState().scissor.enabled
	);

	renderer.FlushBatch();

	TextureDrawParams params{ .size{ display_viewport.size },
							  .tint{ tint },
							  .texture_coordinates{ impl::GetTextureCoordinates(
								  display_viewport.position, display_viewport.size,
								  render_target_size, true, true
							  ) },
							  .effects{ effect_params } };

	// No margin for camera effects so cameras do not exceed viewports
	params.effects.margin = 0;

	draw_context.WithRenderState(
		{ .view_projection = Matrix4::Orthographic(display_viewport.size),
		  .blend_mode	   = BlendMode::ReplaceRGBA },
		[&draw_context, texture, &params, &renderer]() {
			draw_context.DrawTexture({}, texture, std::move(params));

			renderer.FlushBatch();
		}
	);
}

template <typename T>
std::vector<impl::EntityRenderCommand> GetSortedEntityCommands(
	T entity_view, InvocableR<bool, Entity> auto filter
) {
	std::vector<impl::EntityRenderCommand> entity_commands;

	for (auto tuple : entity_view) {
		Entity entity;

		if constexpr (T::with_filter) {
			entity = std::get<0>(tuple);
		} else {
			entity = tuple;
		}

		if (filter(entity)) {
			continue;
		}

		entity_commands.emplace_back(entity, GetDepth(entity));
	}

	std::ranges::stable_sort(entity_commands, [](const auto& a, const auto& b) {
		if (a.depth != b.depth) {
			return a.depth < b.depth;
		}

		return a.entity.WasCreatedBefore(b.entity);
	});

	return entity_commands;
}

void DrawCommands(
	Renderer& renderer, DrawContext& draw_context, auto& manual_commands, auto entities,
	InvocableR<bool, Entity> auto filter, bool debug
) {
	std::size_t entity_index{ 0 };
	std::size_t manual_index{ 0 };

	std::vector<impl::EntityRenderCommand> entity_commands;

	// Debug entity commands not supported.
	if (!debug) {
		entity_commands = GetSortedEntityCommands(entities, filter);
	}

	manual_commands.Sort();

	auto entity_count{ entity_commands.size() };
	auto manual_count{ manual_commands.Count() };

	auto draw_entity = [&]() {
		PTGN_ASSERT(entity_index < entity_commands.size());
		const auto& entity_cmd{ entity_commands[entity_index] };
		PTGN_ASSERT(IsVisible(entity_cmd.entity), "Cannot render entity without visible component");
		impl::InvokeDrawable(draw_context, entity_cmd.entity);
		entity_index++;
	};

	auto draw_command = [&]() {
		manual_commands.Draw(renderer, manual_index);
		manual_index++;
	};

	while (entity_index < entity_count || manual_index < manual_count) {
		if (manual_index >= manual_count) {
			draw_entity();
			continue;
		}

		if (entity_index >= entity_count) {
			draw_command();
			continue;
		}

		PTGN_ASSERT(entity_index < entity_commands.size());

		auto entity_cmd_depth{ entity_commands[entity_index].depth };
		auto manual_cmd_depth{ manual_commands.GetDepth(manual_index) };

		if (entity_cmd_depth <= manual_cmd_depth) {
			draw_entity();
		} else {
			draw_command();
		}
	}

	entity_commands.clear();

	manual_commands.Clear();
}

void DrawCamera(
	Renderer& render, DrawContext& draw_context, auto view, RenderTarget render_target,
	const Camera& camera, std::optional<Color> clear_color, auto& commands, auto& debug_commands,
	Color tint, const impl::EffectParams& effect_params, InvocableR<bool, Entity> auto filter
) {
	auto logical_size{ render.GetLogicalSize() };

	auto render_target_size{ render_target.GetSize() };

	auto display_viewport{ ptgn::GetDisplayViewport(
		camera.raw_viewport, camera.viewport_space, logical_size, render_target_size,
		render_target == render_target.GetScene().GetRenderTarget()
	) };

	impl::RendererAccessor renderer{ render };

	renderer.SetFramebuffer(&render_target.Get<impl::FramebufferObject>());

	renderer.SetViewport(display_viewport);
	renderer.SetViewProjection(camera.view_projection);
	renderer.SetScissor(ScissorState{ display_viewport });

	if (clear_color.has_value()) {
		render_target.ClearColor(clear_color.value(), false);
	}

	DrawCommands(render, draw_context, commands, view, filter, false);

	draw_context.SetBlendMode(BlendMode::Blend);

	DrawCommands(render, draw_context, debug_commands, view, filter, true);

	ApplyCameraEffects(
		render, draw_context, display_viewport, render_target_size, camera.view_projection, tint,
		effect_params
	);
}

void DrawScene(
	Scene& scene, auto& commands, auto& debug_commands, DrawContext& draw_context,
	const RenderTarget& render_target, const Camera& cam, const SceneCamera& camera,
	std::optional<Color> clear_color, Color tint, const impl::EffectParams& effect_params,
	InvocableR<bool, Entity> auto filter
) {
	auto entity_commands{ GetSortedEntityCommands(scene.Entities(), filter) };

	impl::UpdateLightVisibilityPolygons(
		entity_commands, cam.GetWorldVertices(scene.ctx().renderer.GetLogicalSize())
	);

	draw_context.SetBlendMode(BlendMode::Blend);
	impl::DrawDebug(scene, camera, cam, render_target, filter, scene.ctx().debug);

	auto view{ scene.EntitiesWith<impl::IDrawable>() };

	DrawCamera(
		scene.ctx().renderer, draw_context, view, render_target, cam, clear_color, commands,
		debug_commands, tint, effect_params, filter
	);
}

[[nodiscard]] UUID GetSerializedEntityUUID(const json& serialized_entity) {
	PTGN_ASSERT(serialized_entity.is_object(), "Serialized entity must be a JSON object");
	return serialized_entity.at("uuid").get<UUID>();
}

[[nodiscard]] Tag GetSerializedEntityTag(const json& serialized_entity) {
	PTGN_ASSERT(serialized_entity.is_object(), "Serialized entity must be a JSON object");
	return serialized_entity.at("tag").get<Tag>();
}

[[nodiscard]] json SerializeSceneEntity(Entity entity) {
	PTGN_ASSERT(entity, "Cannot serialize a null scene entity");
	PTGN_ASSERT(entity.Has<UUID>(), "Serialized scene entity is missing UUID");
	PTGN_ASSERT(entity.Has<Tag>(), "Serialized scene entity is missing Tag");

	json serialized_entity{
		{ "uuid", entity.Get<UUID>() },
		{ "tag", entity.Get<Tag>() },
	};

	auto tags = SerializeEntityTags(entity);
	if (!tags.empty()) {
		serialized_entity["tags"] = std::move(tags);
	}

	auto components = SerializeEntityComponents(entity);
	if (!components.empty()) {
		serialized_entity["components"] = std::move(components);
	}

	if (HasChildren(entity)) {
		json children = json::array();

		for (Entity child : GetChildren(entity)) {
			PTGN_ASSERT(child, "Cannot serialize a null child entity");
			PTGN_ASSERT(child.Has<UUID>(), "Serialized child entity is missing UUID");
			children.emplace_back(child.Get<UUID>());
		}

		if (!children.empty()) {
			serialized_entity["children"] = std::move(children);
		}
	}

	return serialized_entity;
}

[[nodiscard]] const json* FindSerializedEntity(const json& serialized_entities, UUID uuid) {
	for (const auto& serialized_entity : serialized_entities) {
		if (GetSerializedEntityUUID(serialized_entity) == uuid) {
			return &serialized_entity;
		}
	}

	return nullptr;
}

void ApplySerializedIdentity(Entity entity, const json& serialized_entity) {
	PTGN_ASSERT(entity, "Cannot apply serialized identity to a null entity");
	PTGN_ASSERT(entity.Has<UUID>(), "Created entity is missing UUID");
	PTGN_ASSERT(entity.Has<Tag>(), "Created entity is missing Tag");

	entity.Get<UUID>() = GetSerializedEntityUUID(serialized_entity);
	entity.Get<Tag>() = GetSerializedEntityTag(serialized_entity);
}

} // namespace

Scene::Scene(Scene&& other) noexcept :
	ctx_{ std::exchange(other.ctx_, nullptr) },
	manager_{ std::exchange(other.manager_, {}) },
	data_{ std::exchange(other.data_, {}) },
	asset_dependencies_{ std::exchange(other.asset_dependencies_, {}) } {
	if (ctx_) {
		ctx_->Rebind(*this);
	}
}

Scene& Scene::operator=(Scene&& other) noexcept {
	if (this != &other) {
		ctx_	 = std::exchange(other.ctx_, nullptr);
		manager_ = std::exchange(other.manager_, {});
		data_	 = std::exchange(other.data_, {});
		asset_dependencies_ = std::exchange(other.asset_dependencies_, {});

		if (ctx_) {
			ctx_->Rebind(*this);
		}
	}

	return *this;
}

Scene::~Scene() = default;

void Scene::InitBase(Application& app, impl::SceneData&& scene_data) {
	data_	 = std::move(scene_data);
	ctx_	 = std::make_unique<SceneContext>(app, *this);
}

void Scene::Init(Application& app, impl::SceneData&& scene_data) {
	InitBase(app, std::move(scene_data));
	CreateDefaultSceneEntities();

	{
		impl::AssetCaptureScope capture{
			impl::ApplicationAccessor::ctx(app).assets,
			asset_dependencies_
		};

		OnNew();
	}

	Refresh();
	UpdateRenderTargetSizes(*this);

	auto& app_context{ impl::ApplicationAccessor::ctx(app) };
	
	impl::SaveBootstrapProjectScene(
		app,
		*this,
		app_context.project_bootstrap_save_pending
	);

	OnLoad();
	Refresh();
}

void Scene::Init(Application& app, impl::SceneData&& scene_data, const json& serialized_content) {
	InitBase(app, std::move(scene_data));
	DeserializeContent(serialized_content);
	Refresh();
	OnLoad();
	Refresh();
}

void Scene::CreateDefaultSceneEntities() {
	// Must be created before scene camera.
	ctx_->render_target_ =
		CreateRenderTarget(*this, {}, kDefaultSceneBackgroundColor, kDefaultSceneTargetFormat);
	ctx_->render_target_.Add<Tag>(kDefaultSceneTargetTag);
	ctx_->render_target_.Remove<impl::IDrawable>();

	ctx_->camera = CreateCamera(*this, {}, std::nullopt, ViewportSpace::Logical);
	ctx_->camera.Add<Tag>(kDefaultSceneCameraTag);
	ctx_->fixed_camera_ = CreateCamera(*this, {}, std::nullopt, ViewportSpace::Logical);
	ctx_->fixed_camera_.Add<Tag>(kDefaultSceneFixedCameraTag);
	ctx_->fixed_camera_.SetMasks(
		kDefaultFixedCameraIncludeLayerMask, kDefaultFixedCameraExcludeLayerMask
	);

	SetUI(ctx_->fixed_camera_, true);

	if (data_.first_scene) {
		SetBlendMode(GetRenderTarget(), kDefaultFirstSceneBlendMode);
	}

	Refresh();
}

json Scene::SerializeContent() const {
	json serialized_entities = json::array();

	for (Entity entity : Entities()) {
		serialized_entities.emplace_back(SerializeSceneEntity(entity));
	}

	json primary_entities = json::object();
	primary_entities["render_target"] = GetRenderTarget().Get<UUID>();
	primary_entities["camera"] = GetCamera().Get<UUID>();
	primary_entities["fixed_camera"] = GetFixedCamera().Get<UUID>();

	json content = json::object();
	content["primary_entities"] = std::move(primary_entities);
	content["entities"] = std::move(serialized_entities);

	return content;
}

void Scene::DeserializeContent(const json& serialized_content) {
	PTGN_ASSERT(serialized_content.is_object(), "Serialized scene content must be a JSON object");

	const auto& primary_entities = serialized_content.at("primary_entities");
	const auto& serialized_entities = serialized_content.at("entities");

	PTGN_ASSERT(primary_entities.is_object(), "Serialized primary entities must be a JSON object");
	PTGN_ASSERT(serialized_entities.is_array(), "Serialized scene entities must be a JSON array");

	const UUID render_target_uuid{ primary_entities.at("render_target").get<UUID>() };
	const UUID camera_uuid{ primary_entities.at("camera").get<UUID>() };
	const UUID fixed_camera_uuid{ primary_entities.at("fixed_camera").get<UUID>() };

	PTGN_ASSERT(
		render_target_uuid != camera_uuid && render_target_uuid != fixed_camera_uuid &&
			camera_uuid != fixed_camera_uuid,
		"Serialized primary entity UUIDs must be unique"
	);

	std::vector<UUID> serialized_uuids;
	serialized_uuids.reserve(serialized_entities.size());

	for (const auto& serialized_entity : serialized_entities) {
		PTGN_ASSERT(serialized_entity.is_object(), "Serialized entity must be a JSON object");
		
		PTGN_ASSERT(
			serialized_entity.contains("uuid") &&
				serialized_entity.contains("tag"),
			"Serialized entity must contain uuid and tag"
		);

		if (const auto components{ serialized_entity.find("components") };
			components != serialized_entity.end()) {
			PTGN_ASSERT(
				components->is_object(),
				"Serialized entity components must be a JSON object"
			);
		}

		if (const auto tags{ serialized_entity.find("tags") };
			tags != serialized_entity.end()) {
			PTGN_ASSERT(
				tags->is_array(),
				"Serialized entity tags must be a JSON array"
			);
		}

		if (const auto children{ serialized_entity.find("children") };
			children != serialized_entity.end()) {
			PTGN_ASSERT(
				children->is_array(),
				"Serialized entity children must be a JSON array"
			);
		}

		const UUID uuid{ GetSerializedEntityUUID(serialized_entity) };
		PTGN_ASSERT(
			!std::ranges::contains(serialized_uuids, uuid),
			"Serialized scene contains duplicate entity UUID"
		);
		serialized_uuids.emplace_back(uuid);
	}

	const json* render_target_serialized{
		FindSerializedEntity(serialized_entities, render_target_uuid)
	};
	const json* camera_serialized{ FindSerializedEntity(serialized_entities, camera_uuid) };
	const json* fixed_camera_serialized{
		FindSerializedEntity(serialized_entities, fixed_camera_uuid)
	};

	PTGN_ASSERT(render_target_serialized, "Serialized primary render target entity is missing");
	PTGN_ASSERT(camera_serialized, "Serialized primary camera entity is missing");
	PTGN_ASSERT(fixed_camera_serialized, "Serialized fixed camera entity is missing");

	// Pass 1: construct every entity and assign its persistent UUID and Tag before any component
	// data is deserialized. Primary entities use their specialized constructors so runtime-only
	// renderer and camera components are recreated rather than loaded from JSON.
	ctx_->render_target_ =
		CreateRenderTarget(*this, {}, kDefaultSceneBackgroundColor, kDefaultSceneTargetFormat);
	ApplySerializedIdentity(ctx_->render_target_, *render_target_serialized);
	ctx_->render_target_.Remove<impl::IDrawable>();

	ctx_->camera = CreateCamera(*this, {}, std::nullopt, ViewportSpace::Logical);
	ApplySerializedIdentity(ctx_->camera, *camera_serialized);

	ctx_->fixed_camera_ = CreateCamera(*this, {}, std::nullopt, ViewportSpace::Logical);
	ApplySerializedIdentity(ctx_->fixed_camera_, *fixed_camera_serialized);
	ctx_->fixed_camera_.SetMasks(
		kDefaultFixedCameraIncludeLayerMask, kDefaultFixedCameraExcludeLayerMask
	);
	SetUI(ctx_->fixed_camera_, true);

	if (data_.first_scene) {
		SetBlendMode(GetRenderTarget(), kDefaultFirstSceneBlendMode);
	}

	for (const auto& serialized_entity : serialized_entities) {
		const UUID uuid{ GetSerializedEntityUUID(serialized_entity) };

		if (uuid == render_target_uuid || uuid == camera_uuid || uuid == fixed_camera_uuid) {
			continue;
		}

		CreateEntity(GetSerializedEntityTag(serialized_entity), uuid);
	}

	Refresh();

	// Pass 2: all UUIDs now resolve, so ordinary component deserializers may safely run.
	for (const auto& serialized_entity : serialized_entities) {
		const UUID uuid{ GetSerializedEntityUUID(serialized_entity) };
		Entity entity{ GetEntity(uuid) };

		PTGN_ASSERT(entity, "Failed to find entity created for serialized UUID");

		if (const auto tags{ serialized_entity.find("tags") };
			tags != serialized_entity.end()) {
			DeserializeEntityTags(*tags, entity);
		}

		if (const auto components{ serialized_entity.find("components") };
			components != serialized_entity.end()) {
			DeserializeEntityComponents(*components, entity);
		}
	}

	Refresh();

	// Pass 3: rebuild hierarchy from child UUID lists after all entities and components exist.
	for (const auto& serialized_entity : serialized_entities) {
		const auto children{ serialized_entity.find("children") };
		if (children == serialized_entity.end()) {
			continue;
		}

		Entity parent{ GetEntity(GetSerializedEntityUUID(serialized_entity)) };
		PTGN_ASSERT(parent, "Failed to find serialized hierarchy parent entity");

		for (const auto& serialized_child_uuid : *children) {
			const UUID child_uuid{ serialized_child_uuid.get<UUID>() };
			Entity child{ GetEntity(child_uuid) };

			PTGN_ASSERT(child, "Serialized hierarchy references a missing child UUID");
			PTGN_ASSERT(
				!HasParent(child),
				"Serialized hierarchy assigns the same child to multiple parents"
			);

			AddChild(parent, child);
		}
	}

	Refresh();
	UpdateRenderTargetSizes(*this);
}

void Scene::InternalOnEvent(Event event) {
	if (!data_.runtime) {
		return;
	}

	// Global event, dispatched to all scripted entities and sequence triggers in the scene.
	script_runtime::DispatchGlobalEvent(*this, event);

	if (!event.IsHandled()) {
		OnEvent(event);
	}

	script_runtime::ApplyPending(*this);
}

void Scene::InternalOnEvent() {
	if (!data_.runtime) {
		return;
	}

	auto& events{ ctx().event };

	auto current = std::exchange(events.entity_event_queue_, {});

	for (auto& entity_event : current) {
		Event event{ entity_event.event };

		if (entity_event.entity) {
			Entity entity{ entity_event.entity };

			// Built-in component behavior runs before user-authored scripts and sequence triggers.
			impl::ButtonSystem::OnEvent(entity, event);
			impl::ToggleButtonSystem::OnEvent(entity, event);
			impl::DropdownSystem::OnEvent(entity, event);
			impl::TooltipSystem::OnEvent(entity, event);

			script_runtime::DispatchEvent(entity, event);
			continue;
		}

		InternalOnEvent(event);
	}

	script_runtime::ApplyPending(*this);
}

void Scene::InternalPreUpdate() {
	if (data_.runtime) {
		// Derived UI components first add ButtonData; ButtonSystem then adds Interactive.
		impl::ToggleButtonSystem::Prepare(*this);
		impl::DropdownSystem::Prepare(*this);
		impl::TooltipSystem::Prepare(*this);
		Refresh();

		impl::ButtonSystem::Prepare(*this);
		Refresh();

		ctx().interaction.Update(*this);
	}
}

void Scene::InternalEnter() {
	if (!data_.runtime) {
		return;
	}

	for (auto [entity, _scripts] : EntitiesWith<impl::Scripts>()) {
		script_runtime::AttachAll(entity);
	}
	script_runtime::ApplyPending(*this);

	Refresh();

	OnEnter();
	Refresh();

	for (auto [entity, _scripts] : EntitiesWith<impl::Scripts>()) {
		script_runtime::AttachAll(entity);
	}
	script_runtime::ApplyPending(*this);

	Refresh();
}

bool Scene::IsTransitioning() const {
	return data_.state == impl::SceneState::TransitionIn ||
		   data_.state == impl::SceneState::TransitionOut;
}

bool Scene::IsAwaitingTransitionDelay() const {
	return data_.transition && !data_.transition->IsStarted();
}

void Scene::ClearRenderTargets() {
	impl::RendererAccessor renderer{ ctx().renderer };

	for (auto [entity, framebuffer] : EntitiesWith<impl::FramebufferObject>()) {
		RenderTarget render_target{ entity };

		renderer.SetFramebuffer(&framebuffer);
		renderer.SetViewport(
			{
				.position{},
				.size{ render_target.GetSize() },
			}
		);
		renderer.SetScissor(ScissorState{ false });

		render_target.ClearColor(std::nullopt, false);

		renderer.ClearEntityIds(static_cast<impl::FramebufferId>(framebuffer));
	}
}

void Scene::DrawCameras(DrawContext& draw_context, const std::vector<Entity>& cameras) {
	for (const auto& camera_entity : cameras) {
		SceneCamera camera{ camera_entity };

		auto render_target{ camera.GetRenderTarget() };
		auto tint{ GetTint(camera) };
		auto effect_params{ impl::GetEffectParams(camera) };
		auto cam{ camera.operator Camera() };
		auto clear_color{ camera.GetClearColor() };
		auto filter = [camera](auto entity) {
			return !camera.CanSee(entity) || !IsVisible(entity);
		};

		auto& commands{ ctx().render_queue.GetRenderCommands(camera, false) };
		auto& debug_commands{ ctx().render_queue.GetRenderCommands(camera, true) };

		DrawScene(
			*this, commands, debug_commands, draw_context, render_target, cam, camera, clear_color,
			tint, effect_params, filter
		);
	}
}

void Scene::InternalDraw(DrawContext& draw_context) {
	// The editor may update its presentation viewport after scene update.
	// Synchronize again immediately before rendering to avoid a stretched frame.
	UpdateRenderTargetSizes(*this);

	ClearRenderTargets();

	if (const auto& primary_world_camera{ ctx().renderer.GetPrimaryWorldCamera() };
		primary_world_camera.has_value()) {
		std::vector<Entity> non_scene_cameras;

		for (auto [camera_entity, _data] : EntitiesWith<impl::CameraData>()) {
			impl::RecalculateCameraViewProjection(SceneCamera{ camera_entity });
			if (SceneCamera{ camera_entity }.GetRenderTarget() == ctx_->render_target_) {
				continue;
			}
			non_scene_cameras.emplace_back(camera_entity);
		}

		SortByDepth(non_scene_cameras, false);

		DrawCameras(draw_context, non_scene_cameras);

		ctx().render_queue.CombineCommands();

		PTGN_ASSERT(ctx().render_queue.render_commands_.size() == 1);
		PTGN_ASSERT(ctx().render_queue.debug_commands_.size() == 1);

		SceneCamera camera;
		auto render_target{ GetRenderTarget() };
		auto tint{ color::White };
		impl::EffectParams effect_params;
		Camera cam{ primary_world_camera.value() };
		std::optional<Color> clear_color;
		auto filter = [this](auto entity) {
			auto entity_mask = GetMask(entity);
			auto include	 = ctx().camera.GetIncludeMask();
			auto exclude	 = ctx().camera.GetExcludeMask();

			bool in_include = (entity_mask & include) != 0;
			bool in_exclude = (entity_mask & exclude) != 0;

			return !((in_include && !in_exclude) || IsUI(entity)) || !IsVisible(entity);
		};

		auto& commands{ ctx().render_queue.GetRenderCommands(camera, false) };
		auto& debug_commands{ ctx().render_queue.GetRenderCommands(camera, true) };

		DrawScene(
			*this, commands, debug_commands, draw_context, render_target, cam, camera, clear_color,
			tint, effect_params, filter
		);
	} else {
		std::vector<Entity> cameras;

		for (auto [camera_entity, _data] : EntitiesWith<impl::CameraData>()) {
			impl::RecalculateCameraViewProjection(SceneCamera{ camera_entity });
			cameras.emplace_back(camera_entity);
		}

		SortByDepth(cameras, false);

		DrawCameras(draw_context, cameras);
	}

	impl::RendererAccessor renderer{ ctx().renderer };

	renderer.FlushBatch();

	renderer.SetupPresentationFramebuffer();

	DrawSceneTarget(draw_context);

	renderer.FlushBatch();

	ctx().render_queue.render_commands_.clear();
	ctx().render_queue.debug_commands_.clear();
}

void Scene::DrawSceneTarget(DrawContext& draw_context) const {
	auto texture{ ctx_->render_target_.GetTexture() };
	auto draw_transform{ GetDrawTransform(ctx_->render_target_) };
	auto blend_mode{ GetBlendMode(ctx_->render_target_) };

	auto effects{ impl::GetEffectParams(ctx_->render_target_) };
	// No margin for scene effects so render targets do not exceed sizes.
	effects.margin = 0;

	draw_context.SetBlendMode(blend_mode);
	draw_context.DrawTexture(
		draw_transform, texture,
		{ .size				   = ctx_->render_target_.GetSize(),
		  .tint				   = GetTint(ctx_->render_target_),
		  .texture_coordinates = impl::GetDefaultTextureCoordinates<true>(),
		  .effects			   = std::move(effects) }
	);
}

void Scene::InternalUpdate() {
	script_runtime::ApplyPending(*this);

	if (data_.runtime) {
		InternalRuntimeUpdate();
	}

	InternalMaintenanceUpdate();
}

void Scene::InternalRuntimeUpdate() {
	auto dt{ ctx().dt() };
	script_runtime::Update(*this, dt);

	OnUpdate();

	ParticleEmitter::Update(*this, dt);
	impl::AnimationSystem::Update(*this, dt);
	Lifetime::Update(*this, dt);
	ctx().physics.PreCollisionUpdate();
	ctx().collision.Update(*this, dt);
	ctx().physics.PostCollisionUpdate();
	impl::ButtonSystem::Update(*this);
}

void Scene::InternalMaintenanceUpdate() {
	// Runs for editor and runtime scenes, unlike gameplay-only resize events.
	UpdateRenderTargetSizes(*this);

	for (auto [camera_entity, _data] : EntitiesWith<impl::CameraData>()) {
		impl::ApplyCameraBounds(SceneCamera{ camera_entity });
	}

	Refresh();

	impl::OrphanChildren(*this);
	impl::ClearDeadChildren(*this);
}

void Scene::InternalExit() {
	Refresh();

	if (data_.runtime) {
		OnExit();
		Refresh();
	}

	for (auto [entity, scripts] : EntitiesWith<impl::Scripts>()) {
		scripts.Attach(entity);
		scripts.CancelAll(SequenceCancelReason::OwnerDestroyed);
	}
	script_runtime::ApplyPending(*this);
	Refresh();

	// Clears component hooks.
	manager_.Reset();
	ctx().physics.Reset();
	Refresh();
}

Entity Scene::GetEntity(UUID uuid) const {
	for (const Entity& e : Entities()) {
		PTGN_ASSERT(e.Has<UUID>(), "Entity does not have a valid UUID component");
		if (e.Get<UUID>() == uuid) {
			return e;
		}
	}
	return {};
}

Entity Scene::GetEntity(const Tag& tag) const {
	for (const Entity& e : Entities()) {
		PTGN_ASSERT(e.Has<Tag>(), "Entity does not have a valid Tag component");
		if (e.Get<Tag>().value == tag) {
			return e;
		}
	}
	return {};
}

Entity Scene::CreateEntity(Tag tag, UUID uuid) {
	auto entity{ manager_.CreateEntity() };
	entity.Add<Tag>(std::move(tag));
	entity.Add<UUID>(uuid);
	return Entity{ entity, this };
}

void Scene::SetBackgroundColor(Color background_color) {
	ctx_->render_target_.SetClearColor(background_color);
}

Color Scene::GetBackgroundColor() const {
	return ctx_->render_target_.GetClearColor().value_or(impl::ClearColor{}.color);
}

std::size_t Scene::GetTagHash() const {
	return data_.tag_hash;
}

std::string Scene::GetTag() const {
	return data_.tag;
}

bool Scene::IsRuntime() const {
	return data_.runtime;
}

std::string_view Scene::GetRegisteredType() const {
	return data_.registered_type;
}

RenderTarget Scene::GetRenderTarget() const {
	return ctx_->render_target_;
}

SceneCamera Scene::GetCamera() const {
	return ctx_->camera;
}

SceneCamera Scene::GetFixedCamera() const {
	return ctx_->fixed_camera_;
}

void Scene::AddAssetDependency(AssetKey key) {
	if (key.value.empty() || std::ranges::contains(asset_dependencies_, key)) {
		return;
	}

	asset_dependencies_.emplace_back(std::move(key));
}

const std::vector<AssetKey>& Scene::GetAssetDependencies() const {
	return asset_dependencies_;
}

void Scene::Refresh() {
	manager_.Refresh();
}

std::size_t Scene::GetEntityCount() const {
	return manager_.Size();
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