#pragma once

#include <string_view>
#include <vector>

#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"

namespace ptgn {

namespace impl {

class Renderer;

} // namespace impl

class RenderPassBuilder {
public:
	explicit RenderPassBuilder(impl::Renderer& renderer);

	RenderPassBuilder& Read(TextureSource input);

	RenderPassBuilder& Output(const RenderTargetDesc& output);

	RenderPassBuilder& State(const RenderState& state);

	RenderPassBuilder& ExtraTexture(std::string_view name, TextureSource texture);

	TextureSource Draw(const MaterialState& material);

private:
	impl::Renderer& renderer_;
	TextureSource input_{ impl::BoundTarget{} };
	RenderTargetDesc output_;
	RenderState state_;
	std::vector<impl::TextureBinding> extra_textures_;
};

} // namespace ptgn