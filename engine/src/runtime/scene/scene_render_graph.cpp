#include "runtime/scene/scene_render_graph.h"

#include <compare>
#include <functional>
#include <list>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "algorithm"
#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/draw_context.h"
#include "renderer/pipeline/render_resource.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/render_graph.h"
#include "renderer/render_graph_builder.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture_format.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

namespace impl {

SceneGraphBuilder::SceneGraphBuilder(
	Renderer& renderer, RenderGraph& graph, RenderGraphBuilder& effect_builder
) :
	graph_{ graph }, effect_builder_{ effect_builder }, draw_context_{ renderer, *this } {}

DrawContext& SceneGraphBuilder::GetDrawContext() {
	return draw_context_;
}

void SceneGraphBuilder::ApplyEffect(Entity effect) {
	SealDrawLayer();

	auto effect_name{ effect.Get<EffectName>() };

	const auto& fn = EffectRegistry::Instance().Get(effect_name.value);
	fn(effect_builder_, effect);

	BeginDrawLayer("Draw After " + effect_name.value);
}

struct DrawCommand {
	Entity entity;
	Depth depth;
};

static std::vector<std::pair<RenderCamera, std::optional<SceneCamera>>> CollectRenderCameras(
	ptgn::Scene& scene
) {
	std::vector<std::pair<RenderCamera, std::optional<SceneCamera>>> cameras;

	const auto& primary_world_camera{ scene.ctx().global_renderer_.GetPrimaryWorldCamera() };

	if (primary_world_camera.has_value()) {
		cameras.emplace_back(*primary_world_camera, std::nullopt);
		return cameras;
	}

	for (auto [camera_entity, _camera_data] : scene.EntitiesWith<impl::CameraData>()) {
		SceneCamera camera{ camera_entity };

		impl::RecalculateCameraViewProjection(camera);

		cameras.emplace_back(camera, camera);
	}

	return cameras;
}

static std::vector<DrawCommand> CollectEntityDrawCommands(
	ptgn::Scene& scene, const RenderCamera& camera, std::optional<SceneCamera> scene_camera
) {
	std::vector<DrawCommand> commands;
	commands.reserve(scene.GetEntityCount());

	bool use_camera_visibility_filter{ scene_camera.has_value() };

	for (auto entity : scene.Entities()) {
		if (!entity.Has<impl::Visible, impl::IDrawable>()) {
			continue;
		}

		if (use_camera_visibility_filter && !scene_camera->IsVisible(entity)) {
			continue;
		}

		commands.push_back(DrawCommand{ .entity = entity, .depth = GetDepth(entity) });
	}

	return commands;
}

static void SortDrawCommands(std::vector<impl::DrawCommand>& draw_commands) {
	std::ranges::stable_sort(
		draw_commands,
		[](const impl::DrawCommand& a, const impl::DrawCommand& b) {
			if (a.depth != b.depth) {
				// Keep reverse-depth ordering if the draw loop still iterates backwards.
				return a.depth > b.depth;
			}

			// For same-depth entities, sort newest first so reverse iteration draws
			// older entities first and newer entities last.
			return !a.entity.WasCreatedBefore(b.entity);
		}
	);
}

static void InvokeDrawable(DrawContext& draw_context, Entity entity) {
	PTGN_ASSERT(entity.Has<impl::IDrawable>(), "Cannot render entity without drawable component");
	PTGN_ASSERT(entity.Has<impl::Visible>(), "Cannot render entity without visible component");

	const auto& drawable{ entity.Get<impl::IDrawable>() };

	const auto& drawable_functions{ impl::IDrawable::data() };

	PTGN_ASSERT(drawable_functions.contains(drawable.hash), "Failed to identify drawable hash");

	const auto& draw_function{ drawable_functions.find(drawable.hash)->second };

	draw_function(draw_context, entity);
}

static void BuildCameraDrawNodes(
	ptgn::Scene& scene, RenderGraph& graph, RenderGraphBuilder& effect_builder, Renderer& renderer
) {
	SceneGraphBuilder scene_builder{ renderer, graph, effect_builder };

	for (auto& [camera, scene_camera] : CollectRenderCameras(scene)) {
		auto commands = CollectEntityDrawCommands(scene, camera, scene_camera);
		SortDrawCommands(commands);

		scene_builder.BeginCamera(camera);

		auto& draw = scene_builder.GetDrawContext();

		for (const auto& cmd : commands) {
			InvokeDrawable(draw, cmd.entity);
		}

		scene_builder.EndCamera();
	}
}

static void BuildAttachedEffects(RenderGraphBuilder& builder, std::span<Entity> effects) {
	for (const auto& effect : effects) {
		const auto& effect_name{ effect.Get<EffectName>() };
		const auto& fn = EffectRegistry::Instance().Get(effect_name.value);
		fn(builder, effect);
	}
}

static RenderNode MakePresentNode(Renderer& renderer, TextureNode final_scene, TextureNode screen) {
	RenderNode node;
	node.name = "Present Scene To Screen";
	node.type = RenderNodeType::Present;
	node.texture_bindings.push_back(TextureBinding{ .kind	= TextureBindingKind::BatchSamplerArray,
													.source = final_scene,
													.uniform_name = "u_Textures" });
	node.output			  = TargetNode{ screen.id };
	node.pipeline		  = Hash("texture");
	node.material.shader  = renderer.GetShader("texture");
	node.state.blend_mode = BlendMode::ReplaceRGBA;
	return node;
}

void RenderSceneWithGraph(
	Renderer& renderer, ptgn::Scene& scene, RenderTargetId scene_target, V2_int scene_size,
	TextureFormat scene_format, RenderTargetId screen_target, V2_int screen_size,
	std::span<Entity> scene_attached_effects, std::span<Entity> screen_effects
) {
	RenderGraph graph;

	auto scene_color = graph.ImportTarget("Scene Target", scene_target, scene_size, scene_format);

	RenderGraphBuilder builder{ renderer, graph, scene_color };

	RenderNode clear_scene;
	clear_scene.name   = "Clear Scene Target";
	clear_scene.type   = RenderNodeType::Clear;
	clear_scene.output = TargetNode{ scene_color.id };
	// TODO: Use scene background color.
	clear_scene.clear_color = color::Transparent;
	graph.AddNode(std::move(clear_scene));

	BuildCameraDrawNodes(scene, graph, builder, renderer);
	BuildAttachedEffects(builder, scene_attached_effects);

	auto screen =
		graph.ImportTarget("Screen Target", screen_target, screen_size, TextureFormat::RGBA8);

	auto final_scene = builder.SceneTexture();
	graph.AddNode(MakePresentNode(renderer, final_scene, screen));

	RenderGraphBuilder screen_builder{ renderer, graph, screen };
	BuildAttachedEffects(screen_builder, screen_effects);

	renderer.Execute(graph);
}

void SceneGraphBuilder::SealDrawLayer() {
	if (!draw_layer_open_) {
		return;
	}

	if (!current_draw_packets_.empty()) {
		RenderNode node;
		node.name		  = current_draw_layer_name_;
		node.type		  = RenderNodeType::DrawLayer;
		node.output		  = effect_builder_.SceneTarget();
		node.draw_packets = std::move(current_draw_packets_);

		if (current_camera_.has_value()) {
			node.state.viewport		   = current_camera_->camera.viewport;
			node.state.view_projection = current_camera_->camera.view_projection;
		}

		graph_.AddNode(std::move(node));
	}

	current_draw_packets_.clear();
	current_draw_layer_name_.clear();
	draw_layer_open_ = false;
}

} // namespace impl

} // namespace ptgn