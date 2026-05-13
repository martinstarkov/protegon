#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_resource.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/render_graph.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"

namespace ptgn {

namespace impl {

class Renderer;
class RenderGraph;

} // namespace impl

class PassBuilder {
public:
	PassBuilder(
		impl::Renderer& renderer, impl::RenderGraph& graph, std::string name,
		impl::TargetNode scene_target
	);

	PassBuilder(const PassBuilder&)			   = delete;
	PassBuilder& operator=(const PassBuilder&) = delete;
	PassBuilder(PassBuilder&&)				   = delete;
	PassBuilder& operator=(PassBuilder&&)	   = delete;

	~PassBuilder() {
		PTGN_ASSERT(
			finalized_,
			"PassBuilder was destroyed before Output(), Write(), or WriteScene() was called"
		);
	}

	void AssertNotFinalized() const {
		PTGN_ASSERT(!finalized_, "Cannot modify a finalized render pass");
	}

	void Write(impl::TargetNode target) {
		AssertNotFinalized();
		ValidateWriteTarget(target);

		node_.output = target;

		AddNode();
	}

	void WriteScene() {
		Write(scene_target_);
	}

	PassBuilder& Read(impl::TextureNode texture, std::string_view uniform = "u_Texture") {
		AssertNotFinalized();

		node_.texture_bindings.push_back(impl::TextureBinding{
			.kind		  = impl::TextureBindingKind::NamedSampler,
			.source		  = texture,
			.uniform_name = std::string{ uniform } });

		return *this;
	}

	PassBuilder& ReadTextureArray(
		impl::TextureNode texture, std::string_view uniform = "u_Textures"
	) {
		AssertNotFinalized();

		node_.texture_bindings.push_back(impl::TextureBinding{
			.kind		  = impl::TextureBindingKind::BatchSamplerArray,
			.source		  = texture,
			.uniform_name = std::string{ uniform } });

		return *this;
	}

	PassBuilder& Shader(std::string_view name);

	PassBuilder& Shader(impl::ShaderId shader) {
		AssertNotFinalized();
		node_.material.shader = shader;
		return *this;
	}

	template <typename T>
	PassBuilder& Uniform(std::string_view name, const T& value) {
		AssertNotFinalized();
		UniformValue uniform{ value };

		node_.material.uniforms.emplace_back(UniformWrite{ .name  = std::string{ name },
														   .value = uniform });

		return *this;
	}

	PassBuilder& BlendMode(BlendMode mode) {
		AssertNotFinalized();
		node_.state.blend_mode = mode;
		return *this;
	}

	PassBuilder& Scissor(ScissorState scissor) {
		AssertNotFinalized();
		node_.state.scissor = scissor;
		return *this;
	}

	PassBuilder& Clear(Color color) {
		AssertNotFinalized();
		clear_color_ = color;
		return *this;
	}

	impl::TextureNode OutputLike(impl::TextureNode source, std::string_view name = {});

	impl::TextureNode Output(impl::TextureNode output) {
		AssertNotFinalized();
		ValidateWriteTarget(impl::TargetNode{ output.id });

		node_.output = impl::TargetNode{ output.id };

		AddNode();

		return output;
	}

private:
	void ValidateWriteTarget(impl::TargetNode target) const {
		for (const auto& binding : node_.texture_bindings) {
			auto texture_node = std::get_if<impl::TextureNode>(&binding.source);

			if (!texture_node) {
				continue;
			}

			PTGN_ASSERT(
				texture_node->id != target.id,
				"Render pass reads from the same texture/resource it writes to. "
				"Write to OutputLike(...) first, then use a later pass to write back."
			);
		}
	}

	void AddNode() {
		PTGN_ASSERT(!finalized_, "PassBuilder was already finalized");
		PTGN_ASSERT(node_.output.has_value(), "Pass must have an output before being added");
		PTGN_ASSERT(node_.material.shader, "Pass must have a shader before being added");

		if (clear_color_.has_value()) {
			impl::RenderNode clear;
			clear.name		  = node_.name + " Clear";
			clear.type		  = impl::RenderNodeType::Clear;
			clear.output	  = node_.output;
			clear.clear_color = clear_color_;
			graph_.AddNode(std::move(clear));
		}

		graph_.AddNode(std::move(node_));
		finalized_ = true;
	}

	impl::Renderer& renderer_;
	impl::RenderGraph& graph_;
	impl::RenderNode node_;
	std::optional<Color> clear_color_;

	impl::TargetNode scene_target_;

	bool finalized_{ false };
};

} // namespace ptgn