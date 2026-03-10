#include "runtime/scene/scene.h"

#include <algorithm>
#include <list>
#include <memory>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "app/context.h"
#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/util/span.h"
#include "ecs/ecs.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/render_state.h"
#include "renderer/primitives/render_target.h"
#include "renderer/primitives/texture.h"
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
#include "runtime/physics/collision_handler.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/physics.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scripting/scripts.h"
#include "serialization/json/fwd.h"

namespace ptgn {

SceneEventHandler::SceneEventHandler(Scene& scene) : scene_{ scene } {}

void SceneEventHandler::Emit(EventDispatcher d) {
	scene_.InternalEmit(d);
}

Scene::Scene() : event{ *this }, input{ *this } {}

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
	renderer.Init(*this, ctx_->renderer);

	render_target_ = CreateRenderTarget(
		*this, ResizeMode::DisplaySize, color::Transparent, TextureFormat::RGBA8
	);
	render_target_.Remove<impl::IDrawable>();
	camera		 = CreateCamera(*this);
	fixed_camera = CreateCamera(*this);
	fixed_camera.SetMasks(kLayersNone, kLayersAll);
	SetUI(fixed_camera, true);
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

	draw_function(renderer.GetContext(), entity);
}

void Scene::InternalDraw() {
	auto& global_renderer{ app().renderer };
	auto& render_context{ global_renderer.GetContext() };
	auto game_size{ global_renderer.GetGameSize() };

	for (auto [c, _camera] : EntitiesWith<impl::CameraData>()) {
		Camera cam{ c };
		impl::RecalculateCameraViewProjection(Camera{ cam });

		for (auto [drawable, _v, _d] : EntitiesWith<impl::Visible, impl::IDrawable>()) {
			// Mask test (entity layers vs camera include/exclude).
			if (!cam.IsVisible(drawable)) {
				continue;
			}

			// Frustum culling.
			/*if (!VectorContains(frustum_objects, drawable)) {
				continue;
			}*/

			bool found{ false };

			for (auto& [cmd_cam, cmds] : global_renderer.draw_commands_) {
				if (cmd_cam == cam) {
					cmds.emplace_back(GetDepth(drawable), drawable);
					found = true;
					break;
				}
			}

			if (!found) {
				global_renderer.draw_commands_.emplace_back(
					cam, std::vector<impl::DrawCommand>{ impl::DrawCommand{ GetDepth(drawable),
																			drawable } }
				);
			}
		}
	}

	PTGN_ASSERT((!VectorContainsDuplicates(
		global_renderer.draw_commands_,
		[](const auto& c1, const auto& c2) { return c1.first == c2.first; }
	)));

	impl::EntityDepthCompare compare{ true };

	std::ranges::sort(
		global_renderer.draw_commands_,
		[&](const std::pair<Camera, std::vector<impl::DrawCommand>>& a,
			const std::pair<Camera, std::vector<impl::DrawCommand>>& b) {
			return compare(a.first, b.first);
		}
	);

	for (auto& [cam, cmd] : global_renderer.draw_commands_) {
		RenderTarget render_target;

		if (auto parent_rt = cam.TryGet<impl::ParentRenderTarget>()) {
			render_target = parent_rt->render_target;
		} else {
			render_target = render_target_;
		}

		render_target.Bind();
		render_target.Clear();

		auto rt_size{ render_target.GetSize() };
		auto scale{ V2_float{ rt_size } / game_size };

		auto viewport{ cam.GetViewport() };
		viewport.position = viewport.position * scale;
		viewport.size	  = viewport.size * scale;
		render_context.SetViewport(viewport);
		render_context.SetViewProjection(cam.GetViewProjection());

		if (auto clear_color{ cam.GetClearColor() }; clear_color.has_value()) {
			render_context.SetScissor(ScissorState{ viewport });
			render_target.Clear(*clear_color, false);
			render_context.SetScissor(ScissorState{ false });
		}

		std::ranges::sort(cmd, [&](const impl::DrawCommand& a, const impl::DrawCommand& b) {
			return a.depth < b.depth;
		});

		for (const auto& draw_cmd : cmd) {
			if (std::holds_alternative<Entity>(draw_cmd.payload)) {
				InvokeDrawable(global_renderer, std::get<Entity>(draw_cmd.payload));
			} else {
				std::visit(
					[&](auto& draw) {
						using T = std::decay_t<decltype(draw)>;
						if constexpr (std::is_same_v<T, impl::TextureCommand>) {
							render_context.DrawTexture(
								draw.shader, draw.texture, draw.positions, draw.tint,
								draw_cmd.depth, draw.tex_coords
							);
						} else if constexpr (std::is_same_v<T, impl::QuadCommand>) {
							render_context.DrawQuad(draw.positions, draw.color, draw_cmd.depth);
						} else if constexpr (std::is_same_v<T, impl::LineCommand>) {
							render_context.DrawLine(
								draw.shader, draw.positions, draw.color, draw_cmd.depth
							);
						} else if constexpr (std::is_same_v<T, impl::TriangleCommand>) {
							render_context.DrawTriangle(
								draw.shader, draw.positions, draw.color, draw_cmd.depth
							);
						} else {
							PTGN_ERROR("Invalid draw command type");
						}
					},
					std::get<impl::ManualCommand>(draw_cmd.payload)
				);
			}
		}
	}

	global_renderer.draw_commands_.clear();

	// TODO: Fix.
	/*
	if (collider_visibility_) {
		for (auto [entity, collider] : EntitiesWith<Collider>()) {
			app().debug.DrawShape(
				GetDrawTransform(entity), collider.shape, collider_color_, collider_line_width_,
				GetDrawOrigin(entity), entity.GetCamera()
			);
		}
	}
	*/

	Viewport viewport{ {}, global_renderer.GetDisplayViewport().size };
	auto half_viewport{ viewport.size * 0.5f };

	render_context.BindScreenTarget();
	render_context.SetViewport(viewport);
	render_context.SetViewProjection(Matrix4::Orthographic(-half_viewport, half_viewport));
	render_context.SetBlend(BlendMode::Blend);

	auto transform{ GetDrawTransform(render_target_) };
	auto scene_target_size{ render_target_.GetSize() };
	auto positions{ Rect{ scene_target_size }.GetWorldVertices(transform, Origin::Center) };

	render_context.DrawTexture(
		render_target_, positions, GetTint(render_target_), 0.0f,
		impl::GetDefaultTextureCoordinates(true)
	);
}

void Scene::InternalUpdate() {
	input.Update();

	Refresh();
	OnUpdate();
	Refresh();

	ParticleEmitter::Update(*this);
	Tween::Update(*this, app().DeltaTime());
	impl::AnimationSystem::Update(*this);
	Lifetime::Update(*this);
	physics.PreCollisionUpdate(*this);
	collision_.Update(*this);
	physics.PostCollisionUpdate(*this);
}

void Scene::InternalExit() {
	Refresh();
	OnExit();
	Refresh();
	// Clears component hooks.
	manager_.Reset();
	physics = {};
	//  TODO: Fix.
	// render_target_.Get<GameObject<Camera>>().Reset();
	// fixed_camera.Reset();
	Refresh();
}

// void Scene::ReEnter() {
//	// TODO: Fix.
//	// Application::Get().scene_.Enter(key_);
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

const std::shared_ptr<ApplicationContext>& Scene::GetContext() const {
	return ctx_;
}

ApplicationContext& Scene::app() {
	return *ctx_.get();
}

const ApplicationContext& Scene::app() const {
	return *ctx_.get();
}

} // namespace ptgn