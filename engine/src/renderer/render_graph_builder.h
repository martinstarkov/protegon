#pragma once

#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_pass_builder.h"
#include "renderer/pipeline/render_resource.h"

namespace ptgn {

namespace impl {

class Renderer;
class RenderGraph;

} // namespace impl

class RenderGraphBuilder {
public:
	RenderGraphBuilder(
		impl::Renderer& renderer, impl::RenderGraph& graph, impl::TextureNode scene_texture
	);

	// Canonical readable scene texture.
	// This is the imported scene/screen target texture, not a replaceable logical output.
	impl::TextureNode SceneTexture() const {
		return scene_texture_;
	}

	// Canonical writable scene target.
	impl::TargetNode SceneTarget() const {
		return impl::TargetNode{ scene_texture_.id };
	}

	impl::TextureNode CreateLike(impl::TextureNode source, std::string_view name = {});

	impl::TextureNode Copy(impl::TextureNode source, std::string_view name = {}) {
		auto output = CreateLike(source, name.empty() ? "Copy" : name);

		Pass("Copy")
			.ReadTextureArray(source, "u_Textures")
			.Shader("texture")
			.BlendMode(BlendMode::ReplaceRGBA)
			.Output(output);

		return output;
	}

	PassBuilder Pass(std::string_view name);

	impl::RenderGraph& Graph();

	impl::Renderer& GetRenderer();

private:
	impl::Renderer& renderer_;
	impl::RenderGraph& graph_;

	impl::TextureNode scene_texture_;
};

} // namespace ptgn