#pragma once

#include <string_view>
#include <vector>

#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"

namespace ptgn {

namespace impl {

class Renderer;

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

	PassBuilder& SameSize(TextureFormat format = TextureFormat::RGBA8) {
		auto first = desc_.inputs.front();

		desc_.output = RenderTargetDesc{
			.size	= renderer_.GetTextureSize(first),
			.format = format,
		};

		return *this;
	}

	PassBuilder& HalfSize(TextureFormat format = TextureFormat::RGBA16F) {
		auto first = desc_.inputs.front();

		desc_.output = RenderTargetDesc{
			.size	= renderer_.GetTextureSize(first) / 2,
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

	PassBuilder& Shader(std::string_view name) {
		desc_.material.shader = renderer_.GetShader(name);
		return *this;
	}

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