#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "renderer/resources/framebuffer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"

namespace ptgn {

class Renderer;

namespace impl {

inline constexpr std::size_t kMaxUnusedFramebuffers{ 10 };
/// @brief After this many frames of being unused, a framebuffer will be destroyed.
inline constexpr std::uint64_t kPooledFramebufferFrameLifetime{ 10 };

class FramebufferPool {
public:
	explicit FramebufferPool(Renderer& renderer);

	/// @return True if the framebuffer pool owns the framebuffer, false otherwise.
	[[nodiscard]] bool Owns(FramebufferId framebuffer) const;

	/// @brief NOTE: Caller is responsible for clearing the acquired framebuffer.
	[[nodiscard]] FramebufferId Acquire(TextureDesc desc, std::optional<TextureDesc> other_desc);

	/// @brief Does nothing if the framebuffer is not owned by the pool or is already released.
	void Release(FramebufferId framebuffer);

	/// @brief Destroys all unused framebuffers that have been unused for at least
	/// kPooledFramebufferFrameLifetime frames, and removes them from the pool. Should be
	/// called once per frame.
	void Update();

	FramebufferObject& GetFramebuffer(FramebufferId framebuffer);

private:
	struct PooledFramebuffer {
		FramebufferObject framebuffer{};
		std::uint64_t last_used_frame{ 0 };
		bool used{ false };
	};

	std::uint64_t render_frame_{ 0 };
	Renderer& renderer_;
	std::vector<PooledFramebuffer> pool_;
};

} // namespace impl

} // namespace ptgn