#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "renderer/resources/id.h"
#include "renderer/resources/render_target_object.h"

namespace ptgn {

class Renderer;

namespace impl {

inline constexpr std::size_t kMaxUnusedRenderTargets{ 10 };
/// @brief After this many frames of being unused, a render target will be destroyed.
inline constexpr std::uint64_t kPooledRenderTargetFrameLifetime{ 10 };

class RenderTargetPool {
public:
	explicit RenderTargetPool(Renderer& renderer);

	/// @return True if the render target pool owns the target, false otherwise.
	[[nodiscard]] bool Owns(impl::RenderTargetId id) const;
	[[nodiscard]] impl::RenderTargetId Acquire(RenderTargetDesc desc);

	/// @brief Does nothing if the target is not owned by the pool or is already released.
	void Release(impl::RenderTargetId);

	/// @brief Destroys all unused render targets that have been unused for at least
	/// kPooledRenderTargetFrameLifetime frames, and removes them from the pool. Should be called
	/// once per frame.
	void Update();

	RenderTargetObject Extract(RenderTargetId id);

private:
	struct PooledTarget {
		RenderTargetObject target;
		std::uint64_t last_used_frame{ 0 };
		bool used{ false };
	};

	std::uint64_t render_frame_{ 0 };
	Renderer& renderer_;
	std::vector<PooledTarget> pool_;
};

} // namespace impl

} // namespace ptgn