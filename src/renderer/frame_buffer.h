#pragma once

#include <cstdint>
#include <functional>

#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/surface.h"
#include "renderer/texture.h"
#include "utility/handle.h"

namespace ptgn {

class FrameBuffer;

namespace impl {

struct FrameBufferInstance;

struct RenderBufferInstance {
	RenderBufferInstance();
	~RenderBufferInstance();
	std::uint32_t id_{ 0 };
};

} // namespace impl

class RenderBuffer : public Handle<impl::RenderBufferInstance> {
public:
	RenderBuffer()			 = default;
	~RenderBuffer() override = default;

	// @param size Desired size of the render buffer.
	explicit RenderBuffer(const V2_int& size);

	// @return Id of the currently bound render buffer.
	[[nodiscard]] static std::int32_t GetBoundId();

private:
	friend struct impl::FrameBufferInstance;

	void Bind() const;
	static void Unbind();
};

namespace impl {

struct FrameBufferInstance {
	FrameBufferInstance();
	~FrameBufferInstance();

	void AttachTexture(const Texture& texture);
	void AttachRenderBuffer(const RenderBuffer& render_buffer);

	void Bind() const;

	[[nodiscard]] bool IsBound() const;
	[[nodiscard]] bool IsComplete() const;

	std::uint32_t id_{ 0 };
	Texture texture_;
	RenderBuffer render_buffer_;
};

} // namespace impl

class FrameBuffer : public Handle<impl::FrameBufferInstance> {
public:
	FrameBuffer()			= default;
	~FrameBuffer() override = default;

	explicit FrameBuffer(const Texture& texture);

	explicit FrameBuffer(const RenderBuffer& render_buffer);

	void AttachTexture(const Texture& texture);
	void AttachRenderBuffer(const RenderBuffer& render_buffer);

	// @return The texture attached to the frame buffer.
	[[nodiscard]] Texture GetTexture() const;
	// @return The render buffer attached to the frame buffer.
	[[nodiscard]] RenderBuffer GetRenderBuffer() const;

	// @return True if the frame buffer attachment / creation was successful, false otherwise.
	[[nodiscard]] bool IsComplete() const;
	// @return True if the frame buffer is currently bound to the context, false otherwise.
	[[nodiscard]] bool IsBound() const;

	void Bind() const;

	// Bind 0 as the current frame buffer, used for rendering things to the screen.
	// Necessary for Mac OS as per: https://wiki.libsdl.org/SDL3/SDL_GL_SwapWindow
	static void Unbind();

private:
	// @return Id of the currently bound frame buffer.
	[[nodiscard]] static std::int32_t GetBoundId();
};

} // namespace ptgn