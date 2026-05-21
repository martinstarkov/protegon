#include "renderer/pipeline/render_pass_builder.h"

#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"

namespace ptgn {

PassBuilder::PassBuilder(impl::Renderer& renderer, std::vector<impl::TextureId> inputs) :
	renderer_{ renderer } {
	desc_.inputs = std::move(inputs);
}

PassBuilder& PassBuilder::Shader(std::string_view name) {
	desc_.material.shader = renderer_.GetShader(name);
	return *this;
}

impl::TextureId PassBuilder::Submit() {
	// TODO: Fix.
	return {};
	// return renderer_.SubmitEffectPass(std::move(desc_));
}

} // namespace ptgn