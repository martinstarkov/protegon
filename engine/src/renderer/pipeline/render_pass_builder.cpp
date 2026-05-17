#include "renderer/pipeline/render_pass_builder.h"

namespace ptgn {

/*
PassBuilder::PassBuilder(
	impl::Renderer& renderer, impl::RenderGraph& graph, std::string name,
	impl::TargetNode scene_target
) :
	renderer_{ renderer }, graph_{ graph }, scene_target_{ scene_target } {
	node_.name			   = std::move(name);
	node_.type			   = impl::RenderNodeType::FullscreenPass;
	node_.pipeline		   = Hash("texture");
	node_.state.blend_mode = BlendMode::ReplaceRGBA;
}

impl::TextureNode PassBuilder::OutputLike(impl::TextureNode source, std::string_view name) {
	const auto& src = graph_.Resource(source);

	auto output = graph_.CreateTransient(
		name.empty() ? node_.name + " Output" : std::string{ name }, src.size, src.format
	);

	Output(output);
	return output;
}

PassBuilder& PassBuilder::Shader(std::string_view name) {
	AssertNotFinalized();
	node_.material.shader = renderer_.GetShader(name);
	return *this;
}
*/

} // namespace ptgn