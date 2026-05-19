#include "renderer/pipeline/render_pass_builder.h"

#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/renderer.h"

namespace ptgn {

RenderPassBuilder::RenderPassBuilder(impl::Renderer& renderer) : renderer_{ renderer } {}

RenderPassBuilder& RenderPassBuilder::Read(TextureSource input) {
	input_ = input;
	return *this;
}

RenderPassBuilder& RenderPassBuilder::Output(const RenderTargetDesc& output) {
	output_ = output;
	return *this;
}

RenderPassBuilder& RenderPassBuilder::State(const RenderState& state) {
	state_ = state;
	return *this;
}

RenderPassBuilder& RenderPassBuilder::ExtraTexture(std::string_view name, impl::TextureId texture) {
	extra_textures_.emplace_back(impl::TextureBinding{
		.name	= std::string{ name },
		.source = texture,
	});
	return *this;
}

TextureSource RenderPassBuilder::Draw(const MaterialState& material) {
	return renderer_.DrawPass(material, input_, output_, state_, extra_textures_);
}

} // namespace ptgn