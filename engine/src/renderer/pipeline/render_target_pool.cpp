#include "renderer/pipeline/render_target_pool.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/texture_format.h"

namespace ptgn::impl {

RenderTargetObject::RenderTargetObject(Renderer* renderer, const RenderTargetDesc& desc) :
	Base{ renderer, renderer->CreateRenderTarget(desc) } {}

void RenderTargetObject::Resize(V2_int new_size) {
	PTGN_ASSERT(renderer_ && *this);
	renderer_->ResizeRenderTarget(resource_, new_size);
}

void RenderTargetObject::Clear(Color color, bool set_viewport, bool restore_bind) const {
	PTGN_ASSERT(renderer_ && *this);
	renderer_->ClearRenderTarget(resource_, color, set_viewport, restore_bind);
}

impl::TextureId RenderTargetObject::GetTextureId() const {
	PTGN_ASSERT(renderer_ && *this);
	return renderer_->GetRenderTargetTexture(resource_);
}

void RenderTargetObject::Bind() const {
	PTGN_ASSERT(renderer_ && *this);
	renderer_->SetRenderTarget(this);
}

V2_int RenderTargetObject::GetSize() const {
	PTGN_ASSERT(renderer_ && *this);
	return renderer_->GetRenderTargetSize(resource_);
}

TextureFormat RenderTargetObject::GetFormat() const {
	PTGN_ASSERT(renderer_ && *this);
	return renderer_->GetRenderTargetTextureFormat(resource_);
}

RenderTargetPool::RenderTargetPool(Renderer& renderer) : renderer_{ renderer } {}

RenderTargetObject RenderTargetPool::Acquire(
	RenderTargetDesc desc, std::optional<FramebufferId> exclude
) {
	// TODO: Fix.
	/*
	++tick_;

	PooledTarget* exact			  = nullptr;
	PooledTarget* lru_same_format = nullptr;

	for (auto& entry : pool_) {
		if (entry.in_use) {
			continue;
		}

		if (exclude.has_value() && FramebufferId{ entry.target.operator RenderTargetId() } ==
	*exclude) { continue;
		}

		if (entry.target.GetFormat() != desc.format) {
			continue;
		}

		if (entry.target.GetSize() == desc.size) {
			exact = &entry;
			break;
		}

		if (!lru_same_format || entry.last_used_tick < lru_same_format->last_used_tick) {
			lru_same_format = &entry;
		}
	}

	auto claim = [&](PooledTarget& entry) -> RenderTargetObject& {
		if (entry.target.GetSize() != desc.size) {
			entry.target.Resize(desc.size);
		}

		entry.in_use		 = true;
		entry.last_used_tick = tick_;

		entry.target.Bind();
		entry.target.Clear(color::Transparent, false);

		return entry.target;
	};

	if (exact) {
		return claim(*exact);
	}

	if (lru_same_format) {
		return claim(*lru_same_format);
	}
	PooledTarget created{
		.target			= renderer_.CreateRenderTarget(desc),
		.last_used_tick = tick_,
		.in_use			= true,
	};

	auto& result = pool_.emplace_back(std::move(created));

	*/

	// auto& result = temp_.emplace_back();

	auto result = renderer_.CreateRenderTarget(desc);

	result.Clear(color::Transparent, false, true);

	return std::move(result);
}

RenderTargetObject RenderTargetPool::AcquireLike(const RenderTargetObject& target, int margin) {
	RenderTargetDesc desc{
		.size	= target.GetSize(),
		.format = target.GetFormat(),
	};

	desc.size.x += margin * 2;
	desc.size.y += margin * 2;

	return Acquire(desc, FramebufferId{ target.operator RenderTargetId() });
}

void RenderTargetPool::Release(RenderTargetId id) {
	// TODO: Fix.
	// auto current{ renderer_.GetCurrentTarget() };
	// PTGN_ASSERT(id != current, "Cannot release currently bound render target");
	// for (auto& entry : pool_) {
	//	if (entry.target.operator RenderTargetId() != id) {
	//		continue;
	//	}
	//	PTGN_ASSERT(entry.in_use, "Pooled render target released twice");
	//	entry.in_use		 = false;
	//	entry.last_used_tick = ++tick_;
	//	return;
	//}
	// PTGN_ERROR("Tried to release a render target not owned by RenderTargetPool");
}

bool RenderTargetPool::Owns(RenderTargetId target) const {
	return std::ranges::any_of(temp_, [target](const RenderTargetObject& entry) {
		return entry.operator RenderTargetId() == target;
	});

	// TODO: Fix.
	// return std::ranges::any_of(pool_, [target](const PooledTarget& entry) {
	//	return entry.target.operator RenderTargetId() == target;
	// });
}

void RenderTargetPool::TrimUnused(std::size_t max_unused) {
	temp_.clear();

	// TODO: Fix.
	// std::erase_if(pool_, [&](PooledTarget& entry) {
	//	if (entry.in_use) {
	//		return false;
	//	}
	//	if (pool_.size() <= max_unused) {
	//		return false;
	//	}
	//	renderer_.Destroy(entry.target.operator RenderTargetId());
	//	return true;
	// });
}

} // namespace ptgn::impl