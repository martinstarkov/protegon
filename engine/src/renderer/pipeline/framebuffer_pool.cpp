#include "renderer/pipeline/framebuffer_pool.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/texture_format.h"

namespace ptgn::impl {

FramebufferPool::FramebufferPool(Renderer& renderer) : renderer_{ renderer } {}

void FramebufferPool::Update() {
	std::erase_if(pool_, [this](const auto& entry) {
		if (entry.used) {
			return false;
		}

		bool expired{ entry.last_used_frame + kPooledFramebufferFrameLifetime >= render_frame_ };

		if (bool pool_full{ pool_.size() >= kMaxUnusedFramebuffers }; !expired && !pool_full) {
			return false;
		}

		renderer_.Destroy(entry.framebuffer.operator FramebufferId());
		return true;
	});

	render_frame_++;
}

bool FramebufferPool::Owns(FramebufferId framebuffer) const {
	return std::ranges::any_of(pool_, [framebuffer](const auto& entry) {
		return entry.framebuffer.operator FramebufferId() == framebuffer;
	});
}

FramebufferId FramebufferPool::Acquire(TextureDesc desc, std::optional<TextureDesc> other_desc) {
	PooledFramebuffer* acquired_pool{ nullptr };

	PTGN_ASSERT(
		!other_desc.has_value() || desc != *other_desc,
		"Other texture description cannot match the first one"
	);

	for (auto& entry : pool_) {
		if (entry.used || renderer_.GetFormat(entry.framebuffer) != desc.format) {
			continue;
		}
		if (other_desc.has_value() &&
			!renderer_.GetFormats(entry.framebuffer).HasFormat(other_desc->format)) {
			continue;
		}
		acquired_pool = &entry;
		break;
	}

	if (!acquired_pool) {
		// Create a new framebuffer if no suitable unused framebuffer is found in the pool.

		acquired_pool = &pool_.emplace_back(
			PooledFramebuffer{
				.framebuffer	 = renderer_.CreateFramebuffer(desc, other_desc),
				.last_used_frame = render_frame_,
				.used			 = true,
			}
		);
	} else {
		// Resize and update params of the render target if necessary.

		if (renderer_.GetSize(acquired_pool->framebuffer) != desc.size) {
			renderer_.Resize(acquired_pool->framebuffer, desc.size);
		}

		if (renderer_.GetParams(acquired_pool->framebuffer) != desc.params) {
			renderer_.SetParams(acquired_pool->framebuffer, desc.params);
		}

		acquired_pool->last_used_frame = render_frame_;
		acquired_pool->used			   = true;
	}

	PTGN_ASSERT(acquired_pool, "Failed to acquire a framebuffer from the pool");

	return acquired_pool->framebuffer;
}

void FramebufferPool::Release(FramebufferId framebuffer) {
	std::ranges::for_each(pool_, [this, framebuffer](auto& entry) {
		if (entry.used && entry.framebuffer.operator FramebufferId() == framebuffer) {
			entry.used			  = false;
			entry.last_used_frame = render_frame_;
		}
	});
}

FramebufferObject& FramebufferPool::GetFramebuffer(FramebufferId framebuffer) {
	auto it{ std::ranges::find_if(pool_, [framebuffer](const auto& entry) {
		return entry.framebuffer.operator FramebufferId() == framebuffer;
	}) };
	PTGN_ASSERT(it != pool_.end(), "Framebuffer with id ", framebuffer.value, " not found in pool");
	return it->framebuffer;
}

} // namespace ptgn::impl