#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_packet.h"
#include "renderer/pipeline/render_resource.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

namespace impl {

using RenderNodeId = std::uint32_t;

enum class RenderNodeType {
	DrawLayer,
	FullscreenPass,
	Clear,
	Present
};

using TextureSource = std::variant<TextureId, TextureNode>;

struct TextureBinding {
	TextureBindingKind kind{ TextureBindingKind::NamedSampler };
	TextureSource source{ TextureId{} };
	std::string uniform_name{ "u_Texture" };
};

struct RenderNode {
	RenderNodeId id{ 0 };
	RenderNodeType type{ RenderNodeType::FullscreenPass };
	std::string name;

	std::vector<TextureBinding> texture_bindings;
	std::optional<TargetNode> output;

	RenderState state;
	PipelineId pipeline{ 0 };
	MaterialState material;

	std::vector<RenderPacket> draw_packets;

	std::optional<Color> clear_color;
};

class RenderGraph {
public:
	TextureNode ImportTarget(
		std::string name, RenderTargetId target, V2_int size, TextureFormat format
	) {
		RenderResource resource;
		resource.name	 = std::move(name);
		resource.size	 = size;
		resource.format	 = format;
		resource.storage = ImportedRenderTargetResource{ target };

		auto id = static_cast<RenderResourceId>(resources_.size());
		resources_.push_back(std::move(resource));
		return TextureNode{ id };
	}

	TextureNode CreateTransient(std::string name, V2_int size, TextureFormat format) {
		RenderResource resource;
		resource.name	 = std::move(name);
		resource.size	 = size;
		resource.format	 = format;
		resource.storage = TransientRenderTargetResource{};

		auto id = static_cast<RenderResourceId>(resources_.size());
		resources_.push_back(std::move(resource));
		return TextureNode{ id };
	}

	RenderNodeId AddNode(RenderNode node) {
		node.id = static_cast<RenderNodeId>(nodes_.size());
		nodes_.push_back(std::move(node));
		return nodes_.back().id;
	}

	RenderResource& Resource(TextureNode node) {
		return resources_.at(node.id);
	}

	const RenderResource& Resource(TextureNode node) const {
		return resources_.at(node.id);
	}

	std::vector<RenderResource>& Resources() {
		return resources_;
	}

	const std::vector<RenderResource>& Resources() const {
		return resources_;
	}

	std::vector<RenderNode>& Nodes() {
		return nodes_;
	}

	const std::vector<RenderNode>& Nodes() const {
		return nodes_;
	}

private:
	std::vector<RenderResource> resources_;
	std::vector<RenderNode> nodes_;
};

} // namespace impl

} // namespace ptgn