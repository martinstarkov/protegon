#include "runtime/scene/scene.h"

#include <ecs/ecs.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "app/application.h"
#include "core/assert.h"
#include "core/event/event.h"
#include "core/graphics/color.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/hash.h"
#include "platform/window.h"
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
#include "runtime/animation/tween.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/manager.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/uuid.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/render_queue.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/text/text.h"
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
#include "serialization/json/json.h"

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

void SortEntityDrawCommands(std::vector<impl::EntityRenderCommand>& commands) {
	std::ranges::stable_sort(commands, [](const auto& a, const auto& b) {
		if (a.depth != b.depth) {
			return a.depth < b.depth;
		}

		return a.entity.WasCreatedBefore(b.entity);
	});
}

Viewport GetDisplayViewport(
	const Renderer& renderer, const Camera& camera, RenderTarget render_target
) {
	auto logical_size{ renderer.GetLogicalSize() };

	auto render_target_size{ render_target.GetSize() };

	if (render_target == render_target.GetScene().GetRenderTarget()) {
		render_target_size = renderer.GetDisplaySize();
	}

	auto display_viewport{ ptgn::GetDisplayViewport(
		camera.raw_viewport, camera.viewport_space, logical_size, render_target_size
	) };

	return display_viewport;
}

void SetupCamera(
	Renderer& render, const Matrix4& view_projection, Viewport display_viewport,
	RenderTarget render_target, std::optional<Color> clear_color
) {
	impl::RendererAccessor renderer{ render };

	renderer.SetFramebuffer(&render_target.Get<impl::FramebufferObject>());

	auto offset_viewport{ display_viewport };

	V2_float display_position{ render.GetDisplayPosition() };

	offset_viewport.position += display_position;

	renderer.SetViewport(offset_viewport);
	renderer.SetViewProjection(view_projection);
	renderer.SetScissor(ScissorState{ offset_viewport });

	if (clear_color.has_value()) {
		render_target.ClearColor(clear_color.value(), false);
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

	V2_float display_position{ render.GetDisplayPosition() };

	TextureDrawParams params{ .size{ display_viewport.size },
							  .tint{ tint },
							  .texture_coordinates{ impl::GetTextureCoordinates(
								  display_viewport.position + display_position,
								  display_viewport.size, render_target_size, true, true
							  ) },
							  .effects{ effect_params } };

	// No margin for camera effects so cameras do not exceed their viewports
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

template <InvocableR<bool, Entity> F>
std::vector<impl::EntityRenderCommand> GetSortedEntityCommands(auto entity_view, F&& filter) {
	std::vector<impl::EntityRenderCommand> entity_commands;

	for (auto tuple : entity_view) {
		auto entity{ std::get<0>(tuple) };

		if (filter(entity)) {
			continue;
		}

		entity_commands.emplace_back(entity, GetDepth(entity));
	}

	SortEntityDrawCommands(entity_commands);

	return entity_commands;
}

template <InvocableR<bool, Entity> F>
void DrawCommands(
	Renderer& renderer, DrawContext& draw_context, auto& manual_commands, auto entities, F&& filter,
	bool debug
) {
	std::size_t entity_index{ 0 };
	std::size_t manual_index{ 0 };

	std::vector<impl::EntityRenderCommand> entity_commands;

	// Debug entity commands not supported.
	if (!debug) {
		entity_commands = GetSortedEntityCommands(entities, std::forward<F>(filter));
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

template <InvocableR<bool, Entity> F>
void DrawCommands(
	Renderer& renderer, DrawContext& draw_context, auto view, auto& commands, auto& debug_commands,
	F filter, Viewport display_viewport, V2_float render_target_size,
	const Matrix4& view_projection, Color tint, const impl::EffectParams& effect_params
) {
	DrawCommands(renderer, draw_context, commands, view, filter, false);

	DrawCommands(renderer, draw_context, debug_commands, view, filter, true);

	ApplyCameraEffects(
		renderer, draw_context, display_viewport, render_target_size, view_projection, tint,
		effect_params
	);
}

template <InvocableR<bool, Entity> F>
void DrawCamera(
	Renderer& renderer, DrawContext& draw_context, auto view, RenderTarget render_target,
	const Camera& camera, std::optional<Color> clear_color, auto& commands, auto& debug_commands,
	Color tint, const impl::EffectParams& effect_params, F&& filter
) {
	auto display_viewport{ GetDisplayViewport(renderer, camera, render_target) };

	auto render_target_size{ render_target.GetSize() };

	SetupCamera(renderer, camera.view_projection, display_viewport, render_target, clear_color);

	DrawCommands(
		renderer, draw_context, view, commands, debug_commands, std::forward<F>(filter),
		display_viewport, render_target_size, camera.view_projection, tint, effect_params
	);
}

template <InvocableR<bool, Entity> F>
void DrawScene(
	Scene& scene, auto& commands, auto& debug_commands, DrawContext& draw_context,
	const RenderTarget& render_target, const Camera& cam, const SceneCamera& camera,
	std::optional<Color> clear_color, Color tint, const impl::EffectParams& effect_params,
	F&& filter
) {
	auto light_entity_commands{ GetSortedEntityCommands(
		scene.EntitiesWith<impl::LightData, impl::VisibilityPolygon>(), filter
	) };

	impl::UpdateLightVisibilityPolygons(
		light_entity_commands, cam.GetWorldVertices(scene.ctx().renderer.GetLogicalSize())
	);

	scene.ctx().collision.DrawDebug(scene, camera, filter);
	scene.ctx().interaction.DrawDebug(scene, camera, cam, render_target, filter);
	impl::DrawDebugLightVisibilityPolygons(scene, camera, filter);
	impl::DrawDebugTextBoundingBoxes(scene, camera, filter);

	auto view{ scene.EntitiesWith<impl::Visible, impl::IDrawable>() };

	DrawCamera(
		scene.ctx().renderer, draw_context, view, render_target, cam, clear_color, commands,
		debug_commands, tint, effect_params, filter
	);
}

} // namespace

Scene::Scene(Scene&& other) noexcept :
	ctx_{ std::exchange(other.ctx_, nullptr) },
	manager_{ std::exchange(other.manager_, {}) },
	data_{ std::exchange(other.data_, {}) } {
	if (ctx_) {
		ctx_->Rebind(*this);
	}
}

Scene& Scene::operator=(Scene&& other) noexcept {
	if (this != &other) {
		ctx_	 = std::exchange(other.ctx_, nullptr);
		manager_ = std::exchange(other.manager_, {});
		data_	 = std::exchange(other.data_, {});

		if (ctx_) {
			ctx_->Rebind(*this);
		}
	}

	return *this;
}

Scene::~Scene() = default;

void Scene::Init(Application& app, impl::SceneData&& scene_data) {
	data_ = std::move(scene_data);
	ctx_  = std::make_unique<SceneContext>(app, *this);

	// Must be created before scene camera.
	ctx_->render_target_ =
		CreateRenderTarget(*this, kDefaultSceneBackgroundColor, kDefaultSceneTargetFormat);
	ctx_->render_target_.SetTag(kDefaultSceneTargetTag);
	ctx_->render_target_.Remove<impl::IDrawable>();

	ctx_->camera = CreateCamera(*this);
	ctx_->camera.SetTag(kDefaultSceneCameraTag);
	ctx_->fixed_camera_ = CreateCamera(*this);
	ctx_->fixed_camera_.SetTag(kDefaultSceneFixedCameraTag);
	ctx_->fixed_camera_.SetMasks(
		kDefaultFixedCameraIncludeLayerMask, kDefaultFixedCameraExcludeLayerMask
	);
	SetUI(ctx_->fixed_camera_, true);

	if (data_.first_scene) {
		SetBlendMode(GetRenderTarget(), kDefaultFirstSceneBlendMode);
	}

	Refresh();

	for (auto [e, scripts] : EntitiesWith<impl::Scripts>()) {
		scripts.ApplyPending();
	}

	Refresh();
}

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

void Scene::ClearRenderTargets(DrawContext& draw_context) {
	impl::RendererAccessor renderer{ ctx().renderer };

	draw_context.WithPreservedRenderTarget([this, &renderer]() {
		renderer.SetViewport({ .position = {}, .size = ctx_->render_target_.GetSize() });
		ctx_->render_target_.ClearColor(ctx().window.GetBackgroundColor(), false);
		auto display_viewport{ ctx().renderer.GetDisplayViewport() };
		renderer.SetScissor(ScissorState{ display_viewport });
		ctx_->render_target_.ClearColor(std::nullopt, false);

		for (auto [render_target, frame_buffer, _drawable] :
			 EntitiesWith<impl::FramebufferObject, impl::IDrawable>()) {
			renderer.SetViewport(
				{ .position = {}, .size = RenderTarget{ render_target }.GetSize() }
			);
			renderer.SetScissor(ScissorState{ false });
			RenderTarget{ render_target }.ClearColor(std::nullopt, false);
		}
	});
}

void Scene::DrawCameras(DrawContext& draw_context) {
	std::vector<Entity> camera_entities;

	for (auto [camera_entity, _data] : EntitiesWith<impl::CameraData>()) {
		impl::RecalculateCameraViewProjection(SceneCamera{ camera_entity });
		camera_entities.emplace_back(camera_entity);
	}

	SortByDepth(camera_entities, false);

	for (const auto& camera_entity : camera_entities) {
		SceneCamera camera{ camera_entity };

		auto render_target{ camera.GetRenderTarget() };
		auto tint{ GetTint(camera) };
		auto effect_params{ impl::GetEffectParams(camera) };
		auto cam{ camera.operator Camera() };
		auto clear_color{ camera.GetClearColor() };
		auto filter = [camera](auto entity) {
			return !camera.CanSee(entity);
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
	ClearRenderTargets(draw_context);

	const auto& primary_world_camera{ ctx().renderer.GetPrimaryWorldCamera() };

	if (primary_world_camera.has_value()) {
		ctx().render_queue.CombineCommands();

		PTGN_ASSERT(ctx().render_queue.render_commands_.size() == 1);
		PTGN_ASSERT(ctx().render_queue.debug_commands_.size() == 1);

		SceneCamera camera;
		auto render_target{ GetRenderTarget() };
		auto tint{ color::White };
		impl::EffectParams effect_params;
		Camera cam{ primary_world_camera.value() };
		std::optional<Color> clear_color;
		auto filter = [](auto) {
			return false;
		};

		auto& commands{ ctx().render_queue.GetRenderCommands(camera, false) };
		auto& debug_commands{ ctx().render_queue.GetRenderCommands(camera, true) };

		DrawScene(
			*this, commands, debug_commands, draw_context, render_target, cam, camera, clear_color,
			tint, effect_params, filter
		);
	} else {
		DrawCameras(draw_context);
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
	// No margin for scene effects so render targets do not exceed their sizes.
	effects.margin = 0;

	draw_context.WithBlendMode(
		blend_mode, [this, &draw_context, draw_transform, texture, &effects]() {
			draw_context.DrawTexture(
				draw_transform, texture,
				{ .size				   = ctx_->render_target_.GetSize(),
				  .tint				   = GetTint(ctx_->render_target_),
				  .texture_coordinates = impl::GetDefaultTextureCoordinates<true>(),
				  .effects			   = std::move(effects) }
			);
		}
	);
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
	impl::UpdateButtons(*this);

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

RenderTarget Scene::GetRenderTarget() const {
	return ctx_->render_target_;
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