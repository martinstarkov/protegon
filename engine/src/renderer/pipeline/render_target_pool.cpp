#include "renderer/pipeline/render_target_pool.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"
#include "renderer/resources/texture_format.h"

namespace ptgn::impl {

RenderTargetPool::RenderTargetPool(Renderer& renderer) : renderer_{ renderer } {}

void RenderTargetPool::Update() {
	std::erase_if(pool_, [this](const auto& entry) {
		if (entry.used) {
			return false;
		}

		bool target_expired{ entry.last_used_frame + kPooledRenderTargetFrameLifetime >=
							 render_frame_ };

		if (bool pool_full{ pool_.size() >= kMaxUnusedRenderTargets };
			!target_expired && !pool_full) {
			return false;
		}

		renderer_.Destroy(entry.target.operator RenderTargetId());
		return true;
	});

	render_frame_++;
}

RenderTargetObject RenderTargetPool::Extract(RenderTargetId id) {
	auto it{ std::ranges::find_if(pool_, [id](const PooledTarget& pooled) {
		return pooled.target.operator RenderTargetId() == id;
	}) };

	PTGN_ASSERT(it != pool_.end(), "Cannot extract render target that is not owned by the pool");

	auto extracted{ std::move(it->target) };

	pool_.erase(it);

	return extracted;
}

bool RenderTargetPool::Owns(impl::RenderTargetId id) const {
	return std::ranges::any_of(pool_, [id](const PooledTarget& entry) {
		return entry.target.operator RenderTargetId() == id;
	});
}

impl::RenderTargetId RenderTargetPool::Acquire(RenderTargetDesc desc) {
	PooledTarget* acquired_pool{ nullptr };

	for (auto& entry : pool_) {
		if (entry.used || entry.target.GetFormat() != desc.format) {
			continue;
		}
		acquired_pool = &entry;
		break;
	}

	if (!acquired_pool) {
		// Create a new render target if no suitable unused target is found in the pool.

		acquired_pool = &pool_.emplace_back(
			PooledTarget{
				.target			 = renderer_.CreateRenderTarget(desc),
				.last_used_frame = render_frame_,
				.used			 = true,
			}
		);
	} else {
		// Resize and update params of the render target if necessary.

		if (acquired_pool->target.GetSize() != desc.size) {
			acquired_pool->target.Resize(desc.size);
		}

		if (acquired_pool->target.GetParams() != desc.params) {
			acquired_pool->target.SetParams(desc.params);
		}

		acquired_pool->last_used_frame = render_frame_;
		acquired_pool->used			   = true;
	}

	PTGN_ASSERT(acquired_pool);

	// Clear render target every time it is reacquired.
	acquired_pool->target.Clear(color::Transparent, false, true);

	return acquired_pool->target;
}

void RenderTargetPool::Release(RenderTargetId id) {
	std::ranges::for_each(pool_, [this, id](PooledTarget& entry) {
		if (entry.used && entry.target.operator RenderTargetId() == id) {
			entry.used			  = false;
			entry.last_used_frame = render_frame_;
		}
	});
}

} // namespace ptgn::impl