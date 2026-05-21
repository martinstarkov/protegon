#pragma once

#include <string_view>
#include <utility>
#include <vector>

#include "core/math/vector2.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture_format.h"

namespace ptgn {

namespace impl {

class Renderer;

struct PassDesc {
	std::vector<TextureId> inputs;
	RenderTargetDesc output;
	MaterialState material;
	RenderState render_state;
};

} // namespace impl

class PassBuilder {
public:
	PassBuilder(impl::Renderer& renderer, std::vector<impl::TextureId> inputs);

	PassBuilder& Output(RenderTargetDesc desc) {
		desc_.output = desc;
		return *this;
	}

	PassBuilder& Output(V2_int size, TextureFormat format) {
		desc_.output = RenderTargetDesc{
			.size	= size,
			.format = format,
		};

		return *this;
	}

	PassBuilder& Material(MaterialState material) {
		desc_.material = std::move(material);
		return *this;
	}

	PassBuilder& Shader(impl::ShaderId shader) {
		desc_.material.shader = shader;
		return *this;
	}

	PassBuilder& Shader(std::string_view name);

	PassBuilder& Uniform(UniformWrite uniform) {
		desc_.material.uniforms.push_back(std::move(uniform));
		return *this;
	}

	PassBuilder& State(RenderState state) {
		desc_.render_state = state;
		return *this;
	}

	impl::TextureId Submit();

private:
	impl::Renderer& renderer_;
	impl::PassDesc desc_{};
};

} // namespace ptgn