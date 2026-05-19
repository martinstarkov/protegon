#include "renderer/pipeline/render_batcher.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/assert.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"

namespace ptgn::impl {

RenderBatcher::RenderBatcher(Renderer& renderer) : renderer_{ renderer } {}

void RenderBatcher::EnsureActiveBatchState(
	PipelineId pipeline_id, RenderTargetId target, const MaterialState& material,
	const RenderState& render_state
) {
	if (active_pipeline_id_.has_value() && *active_pipeline_id_ == pipeline_id &&
		active_target_ == target && active_material_ == material &&
		active_render_state_ == render_state) {
		return;
	}

	Flush();

	active_pipeline_id_	 = pipeline_id;
	active_target_		 = target;
	active_material_	 = material;
	active_render_state_ = render_state;
}

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

	PTGN_ASSERT(active_pipeline_id_.has_value());
	PTGN_ASSERT(active_target_);

	const RenderPipeline& pipeline{ GetPipeline(*active_pipeline_id_) };

	renderer_.ApplyRenderTarget(active_target_);

	renderer_.ApplyRenderState(active_render_state_);
	renderer_.ApplyMaterial(active_material_);

	renderer_.UploadVertices(pipeline, vertices_);
	renderer_.UploadIndices(pipeline, indices_);

	for (std::uint32_t slot = 0; slot < textures_.size(); ++slot) {
		renderer_.BindTextureSlot(slot, textures_[slot]);
	}

	renderer_.DrawElements(pipeline, static_cast<std::uint32_t>(indices_.size()));

	vertices_.clear();
	indices_.clear();
	textures_.clear();

	// Important:
	// Do NOT reset active_pipeline_id_, active_target_, active_material_, or
	// active_render_state_. This makes texture-slot overflow and capacity flushes continue
	// naturally.
	ReleaseTargetsAfterFlush();
}

void RenderBatcher::HoldUntilFlush(RenderTargetId target) {
	if (!renderer_.GetTargetPool().Owns(target)) {
		return;
	}

	if (!std::ranges::contains(release_after_flush_, target)) {
		release_after_flush_.push_back(target);
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

const RenderPipeline& RenderBatcher::GetPipeline(std::size_t id) const {
	return renderer_.GetPipeline(id);
}

RenderPipeline& RenderBatcher::GetPipeline(std::size_t id) {
	return renderer_.GetPipeline(id);
}

} // namespace ptgn::impl