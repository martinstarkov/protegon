#pragma once

#include <cstdint>
#include <functional>

#include "math/geometry/polygon.h"
#include "math/matrix4.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/flip.h"
#include "renderer/layer_info.h"
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

	explicit RenderBuffer(const V2_int& size);

	[[nodiscard]] static std::int32_t GetBoundId();

private:
	friend struct impl::FrameBufferInstance;

	void Bind() const;
	static void Unbind();
};

namespace impl {

struct FrameBufferInstance {
	FrameBufferInstance();
	FrameBufferInstance(bool resize_with_window);
	FrameBufferInstance(const V2_float& size);
	~FrameBufferInstance();

	void CreateBlank(const V2_float& size);
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

	// @param continuously_window_sized If true, subscribe to window resize events, else set to
	// current window size.
	explicit FrameBuffer(bool continuously_window_sized);

	explicit FrameBuffer(const V2_float& size);

	explicit FrameBuffer(const Texture& texture);

	explicit FrameBuffer(const RenderBuffer& render_buffer);

	FrameBuffer(const Texture& texture, const RenderBuffer& render_buffer);

	void WhileBound(
		const std::function<void()>& draw_callback, const Color& clear_color = color::Transparent,
		BlendMode blend_mode = BlendMode::Blend
	);

	void ClearToColor(const Color& clear_color);

	void AttachTexture(const Texture& texture);
	void AttachRenderBuffer(const RenderBuffer& render_buffer);

	[[nodiscard]] V2_int GetSize() const;

	[[nodiscard]] Texture GetTexture() const;
	[[nodiscard]] RenderBuffer GetRenderBuffer() const;

	[[nodiscard]] bool IsComplete() const;

	[[nodiscard]] bool IsBound() const;

	void Bind() const;
	static void Unbind();

	// @param destination == {} results in fullscreen shader.
	void Draw(
		const Rect& destination = {}, const M4_float& view_projection = M4_float{ 1.0f },
		const TextureInfo& texture_info = {}
	) const;

	// Draws and unbinds the frame buffer.
	// @param destination == {} results in fullscreen shader.
	void DrawToScreen(
		const Rect& destination = {}, const M4_float& view_projection = M4_float{ 1.0f },
		const TextureInfo& texture_info = {}
	) const;

	[[nodiscard]] Color GetPixel(const V2_int& coordinate) const;

	void ForEachPixel(const std::function<void(V2_int, Color)>& func) const;

	[[nodiscard]] static std::int32_t GetBoundId();

	// Bind a specific frame buffer by id.
	// Note: Will not update the renderer bound frame buffer.
	static void Bind(std::uint32_t id);
};

using RenderTarget = FrameBuffer;

} // namespace ptgn