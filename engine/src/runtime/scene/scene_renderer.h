#pragma once

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "core/util/string.h"
#include "renderer/pipeline/camera.h"
#include "renderer/pipeline/draw_context.h"

#include "renderer/pipeline/render_state.h"

#include "renderer/resources/id.h"
#include "renderer/resources/texture_format.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene_camera.h"

namespace ptgn {

class DrawContext;
class RenderGraphBuilder;
class Scene;

namespace impl {

class Renderer;
class RenderGraph;

class SceneGraphBuilder {
public:
	SceneGraphBuilder(Renderer& renderer, RenderGraph& graph, RenderGraphBuilder& effect_builder);

	DrawContext& GetDrawContext();

	void BeginCamera(const RenderCamera& camera) {
		current_camera_ = camera;
		// TODO: Replace with camera name.
		BeginDrawLayer("Draw " + ToString(camera.uuid));
	}

	void EndCamera() {
		SealDrawLayer();
		current_camera_.reset();
	}

	void Append(RenderPacket packet) {
		EnsureDrawLayer();
		current_draw_packets_.push_back(std::move(packet));
	}

	void ApplyEffect(Entity effect);

private:
	void EnsureDrawLayer() {
		if (!draw_layer_open_) {
			BeginDrawLayer("Draw Layer");
		}
	}

	void BeginDrawLayer(std::string name) {
		PTGN_ASSERT(!draw_layer_open_, "Draw layer already open");
		draw_layer_open_		 = true;
		current_draw_layer_name_ = std::move(name);
		current_draw_packets_.clear();
	}

	void SealDrawLayer();

private:
	RenderGraph& graph_;
	RenderGraphBuilder& effect_builder_;
	DrawContext draw_context_;

	std::optional<RenderCamera> current_camera_;

	bool draw_layer_open_{ false };
	std::string current_draw_layer_name_;
	std::vector<RenderPacket> current_draw_packets_;
};

void RenderSceneWithGraph(
	Renderer& renderer, ptgn::Scene& scene, RenderTargetId scene_target, V2_int scene_size,
	TextureFormat scene_format, RenderTargetId screen_target, V2_int screen_size,
	std::span<Entity> scene_attached_effects, std::span<Entity> screen_effects
);

} // namespace impl

} // namespace ptgn