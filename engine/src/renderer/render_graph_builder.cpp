#include "renderer/render_graph_builder.h"

#include <string>
#include <string_view>

#include "pipeline/render_pass_builder.h"
#include "renderer/pipeline/render_resource.h"
#include "renderer/render_graph.h"
#include "renderer/renderer.h"

namespace ptgn {

RenderGraphBuilder::RenderGraphBuilder(
	impl::Renderer& renderer, impl::RenderGraph& graph, impl::TextureNode scene_texture
) :
	renderer_{ renderer }, graph_{ graph }, scene_texture_{ scene_texture } {}

PassBuilder RenderGraphBuilder::Pass(std::string_view name) {
	return PassBuilder{ renderer_, graph_, std::string{ name }, SceneTarget() };
}

impl::RenderGraph& RenderGraphBuilder::Graph() {
	return graph_;
}

impl::Renderer& RenderGraphBuilder::GetRenderer() {
	return renderer_;
}

impl::TextureNode RenderGraphBuilder::CreateLike(impl::TextureNode source, std::string_view name) {
	const auto& src = graph_.Resource(source);

	return graph_.CreateTransient(
		name.empty() ? src.name + " Transient" : std::string{ name }, src.size, src.format
	);
}

} // namespace ptgn