#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"

namespace ptgn::impl {

class Renderer;

class RenderTargetPool {
public:
	explicit RenderTargetPool(Renderer& renderer);

	RenderTargetObject Acquire(
		RenderTargetDesc desc, std::optional<FramebufferId> exclude = std::nullopt
	);

	RenderTargetObject AcquireLike(const RenderTargetObject& target, int margin = 0);

	void Release(RenderTargetId id);

	[[nodiscard]] bool Owns(RenderTargetId target) const;

	void TrimUnused(std::size_t max_unused);

private:
	struct PooledTarget {
		RenderTargetObject target;
		std::uint64_t last_used_tick{ 0 };
		bool in_use{ false };
	};

	Renderer& renderer_;
	std::vector<PooledTarget> pool_;
	std::vector<RenderTargetObject> temp_;
	std::uint64_t tick_{ 0 };
};

} // namespace ptgn::impl