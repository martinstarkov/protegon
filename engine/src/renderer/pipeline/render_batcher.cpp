#include "renderer/pipeline/render_batcher.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"

namespace ptgn::impl {

RenderBatcher::RenderBatcher(Renderer& renderer) : renderer_{ renderer } {}

RenderBatcher::TextureSlotInfo RenderBatcher::GetTextureSlotNoFlush(TextureId texture) const {
	for (std::uint32_t i = 0; i < textures_.size(); ++i) {
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
		ReleaseTargetsAfterFlush();
		return;
	}

	const RenderPipeline& pipeline{ renderer_.pipeline_manager_.GetCurrentPipeline() };

	renderer_.UploadVertices(pipeline, vertices_);
	renderer_.UploadIndices(pipeline, indices_);

	for (std::uint32_t slot = 0; slot < textures_.size(); ++slot) {
		renderer_.BindTextureSlot(slot, textures_[slot]);
	}

	renderer_.DrawElements(pipeline, static_cast<std::uint32_t>(indices_.size()));

	vertices_.clear();
	indices_.clear();
	textures_.clear();

	ReleaseTargetsAfterFlush();
}

void RenderBatcher::HoldUntilFlush(RenderTargetObject target) {
	if (!renderer_.GetTargetPool().Owns(target)) {
		return;
	}

	if (!std::ranges::contains(release_after_flush_, target)) {
		release_after_flush_.emplace_back(std::move(target));
	}
}

void RenderBatcher::ReleaseTargetsAfterFlush() {
	for (const auto& target : release_after_flush_) {
		renderer_.GetTargetPool().Release(target);
	}

	release_after_flush_.clear();
}

bool RenderBatcher::IsTextureAttachedToCurrentFramebuffer(TextureId texture) const {
	return renderer_.IsTextureAttachedToCurrentFramebuffer(texture);
}

std::size_t RenderBatcher::GetMaxTextureSlots() const {
	return renderer_.GetMaxTextureSlots();
}

} // namespace ptgn::impl