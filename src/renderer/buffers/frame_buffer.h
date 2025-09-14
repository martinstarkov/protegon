#pragma once

#include <cstdint>
#include <functional>

#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/texture.h"
#include "serialization/enum.h"

namespace ptgn::impl {

using RenderBufferId = std::uint32_t;

enum class FrameBufferAttachment {
	DepthStencil = 0x821A, //  GL_DEPTH_STENCIL
	Depth		 = 0x8D00, // GL_DEPTH_COMPONENT
	Stencil		 = 0x8D20, //  GL_STENCIL_INDEX
	Color0		 = 0x8CE0, // GL_COLOR_ATTACHMENT0
	Color1		 = 0x8CE1, // GL_COLOR_ATTACHMENT1
	Color2		 = 0x8CE2  // GL_COLOR_ATTACHMENT2
};

class RenderBuffer {
public:
	RenderBuffer() = default;
	// @param size Desired size of the render buffer.
	explicit RenderBuffer(const V2_int& size, InternalGLFormat format);

	RenderBuffer(const RenderBuffer&)			 = delete;
	RenderBuffer& operator=(const RenderBuffer&) = delete;
	RenderBuffer(RenderBuffer&& other) noexcept;
	RenderBuffer& operator=(RenderBuffer&& other) noexcept;

	~RenderBuffer();

	// @return Id of the currently bound render buffer.
	[[nodiscard]] static RenderBufferId GetBoundId();

	// Bind a specific id as the current render buffer.
	static void Bind(RenderBufferId id);

	void Bind() const;

	static void Unbind();

	// @return The id of the render buffer.
	[[nodiscard]] RenderBufferId GetId() const;

	// @return True if id != 0.
	[[nodiscard]] bool IsValid() const;

private:
	void GenerateRenderBuffer();
	void DeleteRenderBuffer() noexcept;

	RenderBufferId id_{ 0 };
};

using FrameBufferId = std::uint32_t;

class FrameBuffer {
public:
	FrameBuffer() = default;

	explicit FrameBuffer(Texture&& texture);

	FrameBuffer(const FrameBuffer&)			   = delete;
	FrameBuffer& operator=(const FrameBuffer&) = delete;
	FrameBuffer(FrameBuffer&& other) noexcept;
	FrameBuffer& operator=(FrameBuffer&& other) noexcept;
	~FrameBuffer();

	void AttachTexture(Texture&& texture, FrameBufferAttachment attachment);

	void AttachRenderBuffer(RenderBuffer&& render_buffer, FrameBufferAttachment attachment);

	// @return The texture attached to the frame buffer.
	[[nodiscard]] const Texture& GetTexture() const;
	[[nodiscard]] Texture& GetTexture();

	// @return The render buffer attached to the frame buffer.
	[[nodiscard]] const RenderBuffer& GetRenderBuffer() const;

	// @return True if the frame buffer attachment / creation was successful, false otherwise.
	[[nodiscard]] bool IsComplete() const;

	[[nodiscard]] const char* GetStatus() const;

	// Bind a specific id as the current frame buffer.
	// Note: Calling this outside of the FrameBuffer class may mess with the renderer as it keeps
	// track of the currently bound frame buffer.
	static void Bind(FrameBufferId id);

	void Bind() const;

	// Bind 0 as the current frame buffer, used for rendering things to the screen.
	// Necessary for Mac OS as per: https://wiki.libsdl.org/SDL3/SDL_GL_SwapWindow
	static void Unbind();

	// @return Id of the currently bound frame buffer.
	[[nodiscard]] static FrameBufferId GetBoundId();

	// @return True if the frame buffer is currently bound to the context, false otherwise.
	[[nodiscard]] bool IsBound() const;

	// @return True if the current bound frame buffer id is 0, false otherwise.
	[[nodiscard]] static bool IsUnbound();

	void ClearToColor(const Color& color) const;

	// @return True if id != 0.
	[[nodiscard]] bool IsValid() const;

	// @return The id of the frame buffer.
	[[nodiscard]] FrameBufferId GetId() const;

	// WARNING: This function is slow and should be
	// primarily used for debugging frame buffers.
	// @param coordinate Pixel coordinate from [0, size).
	// @param restore_bind_state If true, rebinds the previously bound frame buffer and texture ids.
	// @return Color value of the given pixel.
	// Note: Only RGB/RGBA format textures supported.
	[[nodiscard]] Color GetPixel(const V2_int& coordinate, bool restore_bind_state = true) const;

	// WARNING: This function is slow and should be
	// primarily used for debugging frame buffers.
	// @param callback Function to be called for each pixel.
	// @param restore_bind_state If true, rebinds the previously bound frame buffer and texture ids.
	// Note: Only RGB/RGBA format textures supported.
	void ForEachPixel(
		const std::function<void(V2_int, Color)>& callback, bool restore_bind_state = true
	) const;

private:
	void GenerateFrameBuffer();
	void DeleteFrameBuffer() noexcept;

	FrameBufferId id_{ 0 };
	Texture texture_;
	RenderBuffer render_buffer_;
};

PTGN_SERIALIZER_REGISTER_ENUM(
	FrameBufferAttachment, { { FrameBufferAttachment::DepthStencil, "depth_stencil" },
							 { FrameBufferAttachment::Stencil, "stencil" },
							 { FrameBufferAttachment::Depth, "depth" },
							 { FrameBufferAttachment::Color0, "color0" },
							 { FrameBufferAttachment::Color1, "color1" },
							 { FrameBufferAttachment::Color2, "color2" } }
);

} // namespace ptgn::impl