#include "renderer/pipeline/render_batcher.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl {

RenderBatcher::RenderBatcher(Renderer& renderer) : renderer_{ renderer } {}

void RenderBatcher::BindTextureUniforms(
	std::span<const TextureBinding> bindings, std::span<const TextureId> textures
) {
	PTGN_ASSERT(bindings.size() == textures.size(), "Texture bindings must match texture count");

	for (auto i{ 0uz }; i < textures.size(); ++i) {
		const auto& binding{ bindings[i] };
		auto texture{ textures[i] };

		PTGN_ASSERT(texture, "Cannot bind an invalid texture");
		PTGN_ASSERT(binding.slot < GetMaxTextureSlots(), "Texture slot is out of range");
		PTGN_ASSERT(!binding.uniform.empty(), "Texture uniform name cannot be empty");

		if (IsTextureAttachedToCurrentFramebuffer(texture)) {
			PTGN_ERROR("Cannot sample from a texture attached to the current framebuffer");
		}

		renderer_.BindTextureSlot(binding.slot, texture);
		renderer_.SetBoundShaderUniform(binding.uniform.c_str(), static_cast<int>(binding.slot));
	}
}

RenderBatcher::TextureSlotInfo RenderBatcher::GetTextureSlotNoFlush(TextureId texture) const {
	for (auto i{ 0u }; i < textures_.size(); ++i) {
		if (textures_[i] == texture) {
			return {
				.slot		   = i,
				.push_to_batch = false,
			};
		}
	}

	return {
		.slot		   = static_cast<std::uint32_t>(textures_.size()),
		.push_to_batch = true,
	};
}

void RenderBatcher::Flush() {
	if (indices_.empty()) {
		return;
	}

	const RenderPipeline& pipeline{ renderer_.pipeline_manager_.GetCurrentPipeline() };

	renderer_.UploadVertices(pipeline, vertices_, vertex_size_);
	renderer_.UploadIndices(pipeline, indices_);

	for (auto i{ 0u }; i < textures_.size(); ++i) {
		renderer_.BindTextureSlot(i, textures_[i]);
	}

	renderer_.DrawElements(pipeline, static_cast<std::uint32_t>(indices_.size()));

	vertex_size_ = 0;
	vertices_.clear();
	indices_.clear();
	textures_.clear();
}

bool RenderBatcher::IsTextureAttachedToCurrentFramebuffer(TextureId texture) const {
	return renderer_.IsTextureAttachedToCurrentFramebuffer(texture);
}

std::size_t RenderBatcher::GetMaxTextureSlots() const {
	return renderer_.GetMaxTextureSlots();
}

} // namespace ptgn::impl