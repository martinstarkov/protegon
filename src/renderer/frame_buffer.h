#pragma once

#include <cstdint>
#include <functional>

#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/texture.h"

namespace ptgn::impl {

class RenderBuffer {
public:
	RenderBuffer() = default;
	// @param size Desired size of the render buffer.
	explicit RenderBuffer(const V2_int& size);

	RenderBuffer(const RenderBuffer&)			 = delete;
	RenderBuffer& operator=(const RenderBuffer&) = delete;
	RenderBuffer(RenderBuffer&& other) noexcept;
	RenderBuffer& operator=(RenderBuffer&& other) noexcept;

	~RenderBuffer();

	// @return Id of the currently bound render buffer.
	[[nodiscard]] static std::uint32_t GetBoundId();

	// Bind a specific id as the current render buffer.
	static void Bind(std::uint32_t id);

	void Bind() const;

	static void Unbind();

	// @return The id of the render buffer.
	[[nodiscard]] std::uint32_t GetId() const;

	// @return True if id != 0.
	[[nodiscard]] bool IsValid() const;

private:
	void GenerateRenderBuffer();
	void DeleteRenderBuffer() noexcept;

	std::uint32_t id_{ 0 };
};

class FrameBuffer {
public:
	FrameBuffer() = default;

	explicit FrameBuffer(Texture&& texture);

	explicit FrameBuffer(RenderBuffer&& render_buffer);

	FrameBuffer(const FrameBuffer&)			   = delete;
	FrameBuffer& operator=(const FrameBuffer&) = delete;
	FrameBuffer(FrameBuffer&& other) noexcept;
	FrameBuffer& operator=(FrameBuffer&& other) noexcept;
	~FrameBuffer();

	void AttachTexture(Texture&& texture);

	void AttachRenderBuffer(RenderBuffer&& render_buffer);

	// @return The texture attached to the frame buffer.
	[[nodiscard]] const Texture& GetTexture() const;
	[[nodiscard]] Texture& GetTexture();

	// @return The render buffer attached to the frame buffer.
	[[nodiscard]] const RenderBuffer& GetRenderBuffer() const;

	// @return True if the frame buffer attachment / creation was successful, false otherwise.
	[[nodiscard]] bool IsComplete() const;

	// Bind a specific id as the current frame buffer.
	// Note: Calling this outside of the FrameBuffer class may mess with the renderer as it keeps
	// track of the currently bound frame buffer.
	static void Bind(std::uint32_t id);

	void Bind() const;

	// Bind 0 as the current frame buffer, used for rendering things to the screen.
	// Necessary for Mac OS as per: https://wiki.libsdl.org/SDL3/SDL_GL_SwapWindow
	static void Unbind();

	// @return Id of the currently bound frame buffer.
	[[nodiscard]] static std::uint32_t GetBoundId();

	// @return True if the frame buffer is currently bound to the context, false otherwise.
	[[nodiscard]] bool IsBound() const;

	// @return True if the current bound frame buffer id is 0, false otherwise.
	[[nodiscard]] static bool IsUnbound();

	// @return True if id != 0.
	[[nodiscard]] bool IsValid() const;

	// WARNING: This function is slow and should be primarily used for debugging frame buffers.
	// @param coordinate Pixel coordinate from [0, size).
	// @return Color value of the given pixel.
	// Note: Only RGB/RGBA format textures supported.
	[[nodiscard]] Color GetPixel(const V2_int& coordinate) const;

	// WARNING: This function is slow and should be primarily used for debugging frame buffers.
	// @param callback Function to be called for each pixel.
	// Note: Only RGB/RGBA format textures supported.
	void ForEachPixel(const std::function<void(V2_int, Color)>& callback) const;

private:
	void GenerateFrameBuffer();
	void DeleteFrameBuffer() noexcept;

	std::uint32_t id_{ 0 };
	Texture texture_;
	RenderBuffer render_buffer_;
};

} // namespace ptgn::impl