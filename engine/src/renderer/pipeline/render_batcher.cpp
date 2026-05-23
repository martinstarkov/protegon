#include "renderer/pipeline/render_batcher.h"

#include <array>
#include <cstdint>
#include <vector>

#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/math/vector4.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/pipeline/render_target_pool.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/vertex/vertex.h"

namespace ptgn::impl {

RenderQuad<TextureVertex> CreateRenderQuad(
	const std::array<V2_float, 4>& positions, float depth, V4_float color_n,
	const std::array<V2_float, 4>& tex_coords, float tex_index, int entity_id
) {
	return {
		TextureVertex{ positions[0], depth, color_n, tex_coords[0], tex_index, entity_id },
		TextureVertex{ positions[1], depth, color_n, tex_coords[1], tex_index, entity_id },
		TextureVertex{ positions[2], depth, color_n, tex_coords[2], tex_index, entity_id },
		TextureVertex{ positions[3], depth, color_n, tex_coords[3], tex_index, entity_id },
	};
}

RenderQuad<TextureVertex> CreateLocalRenderQuad(
	Rect rect, float depth, V4_float color_n, const std::array<V2_float, 4>& tex_coords,
	float tex_index, int entity_id
) {
	auto positions{ rect.GetLocalVertices() };

	return {
		TextureVertex{ positions[0], depth, color_n, tex_coords[0], tex_index, entity_id },
		TextureVertex{ positions[1], depth, color_n, tex_coords[1], tex_index, entity_id },
		TextureVertex{ positions[2], depth, color_n, tex_coords[2], tex_index, entity_id },
		TextureVertex{ positions[3], depth, color_n, tex_coords[3], tex_index, entity_id },
	};
}

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

	for (auto i{ 0u }; i < textures_.size(); ++i) {
		renderer_.BindTextureSlot(i, textures_[i]);
	}

	renderer_.DrawElements(pipeline, static_cast<std::uint32_t>(indices_.size()));

	vertices_.clear();
	indices_.clear();
	textures_.clear();

	ReleaseTargetsAfterFlush();
}

void RenderBatcher::HoldUntilFlush(RenderTargetObject target) {
	// TODO: Fix.
	// if (!renderer_.GetTargetPool().Owns(target)) {
	//	return;
	//}

	// if (!std::ranges::contains(release_after_flush_, target)) {
	//	release_after_flush_.emplace_back(std::move(target));
	// }
}

void RenderBatcher::ReleaseTargetsAfterFlush() {
	// TODO: Fix.
	// for (const auto& target : release_after_flush_) {
	//	renderer_.GetTargetPool().Release(target);
	//}

	// release_after_flush_.clear();
}

bool RenderBatcher::IsTextureAttachedToCurrentFramebuffer(TextureId texture) const {
	return renderer_.IsTextureAttachedToCurrentFramebuffer(texture);
}

std::size_t RenderBatcher::GetMaxTextureSlots() const {
	return renderer_.GetMaxTextureSlots();
}

} // namespace ptgn::impl