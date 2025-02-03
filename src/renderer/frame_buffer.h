#pragma once

#include <cstdint>

#include "math/vector2.h"
#include "renderer/texture.h"
#include "utility/handle.h"

namespace ptgn {

class FrameBuffer;
class RenderTarget;
class Texture;
class Shader;

namespace impl {

class Renderer;
struct FrameBufferInstance;
struct RenderTargetInstance;

struct RenderBufferInstance {
	RenderBufferInstance();
	RenderBufferInstance(const RenderBufferInstance&)			 = default;
	RenderBufferInstance& operator=(const RenderBufferInstance&) = default;
	RenderBufferInstance(RenderBufferInstance&&)				 = default;
	RenderBufferInstance& operator=(RenderBufferInstance&&)		 = default;
	~RenderBufferInstance();

	std::uint32_t id_{ 0 };
};

} // namespace impl

class RenderBuffer : public Handle<impl::RenderBufferInstance> {
public:
	RenderBuffer() = default;

	// @param size Desired size of the render buffer.
	explicit RenderBuffer(const V2_int& size);

	// @return Id of the currently bound render buffer.
	[[nodiscard]] static std::uint32_t GetBoundId();

private:
	friend struct impl::FrameBufferInstance;

	// Bind a specific id as the current render buffer.
	static void Bind(std::uint32_t id);

	void Bind() const;
	static void Unbind();
};

namespace impl {

struct FrameBufferInstance {
	FrameBufferInstance();
	FrameBufferInstance(const FrameBufferInstance&)			   = default;
	FrameBufferInstance& operator=(const FrameBufferInstance&) = default;
	FrameBufferInstance(FrameBufferInstance&&)				   = default;
	FrameBufferInstance& operator=(FrameBufferInstance&&)	   = default;
	~FrameBufferInstance();

	void AttachTexture(TextureManager::Texture texture);
	void AttachRenderBuffer(const RenderBuffer& render_buffer);

	[[nodiscard]] bool IsBound() const;
	[[nodiscard]] bool IsComplete() const;

	std::uint32_t id_{ 0 };
	TextureManager::Texture texture_;
	RenderBuffer render_buffer_;
};

} // namespace impl

class FrameBuffer : public Handle<impl::FrameBufferInstance> {
public:
	FrameBuffer() = default;

	explicit FrameBuffer(
		impl::TextureManager::Texture texture, bool rebind_previous_frame_buffer = true
	);

	explicit FrameBuffer(
		const RenderBuffer& render_buffer, bool rebind_previous_frame_buffer = true
	);

	void AttachTexture(impl::TextureManager::Texture texture);
	void AttachRenderBuffer(const RenderBuffer& render_buffer);

	// @return The texture attached to the frame buffer.
	[[nodiscard]] const impl::TextureManager::Texture& GetTexture() const;
	// @return The render buffer attached to the frame buffer.
	[[nodiscard]] RenderBuffer GetRenderBuffer() const;

	// @return True if the frame buffer attachment / creation was successful, false otherwise.
	[[nodiscard]] bool IsComplete() const;

	// @return True if the frame buffer is currently bound to the context, false otherwise.
	[[nodiscard]] bool IsBound() const;

	// @return True if the current bound frame buffer id is 0, false otherwise.
	[[nodiscard]] static bool IsUnbound();

	void Bind() const;

	// Bind 0 as the current frame buffer, used for rendering things to the screen.
	// Necessary for Mac OS as per: https://wiki.libsdl.org/SDL3/SDL_GL_SwapWindow
	static void Unbind();

private:
	friend struct impl::FrameBufferInstance;
	friend struct impl::RenderTargetInstance;
	friend class impl::Renderer;
	friend class Shader;
	friend class RenderTarget;
	friend class Texture;

	// @return Id of the currently bound frame buffer.
	[[nodiscard]] static std::uint32_t GetBoundId();

	// Bind a specific id as the current frame buffer.
	// Note: Calling this outside of the FrameBuffer class may mess with the renderer as it keeps
	// track of the currently bound frame buffer.
	static void Bind(std::uint32_t id);
};

} // namespace ptgn