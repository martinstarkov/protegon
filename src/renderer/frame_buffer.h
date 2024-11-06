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
class Texture;

namespace impl {

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

private:
	friend class FrameBuffer;

	[[nodiscard]] static std::int32_t GetBoundId();

	void Bind() const;
	static void Unbind();
};

namespace impl {

struct FrameBufferInstance {
	FrameBufferInstance();
	~FrameBufferInstance();
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
	FrameBuffer(const Texture& texture, const RenderBuffer& render_buffer);

	void AttachTexture(const Texture& texture);
	void AttachRenderBuffer(const RenderBuffer& render_buffer);

	[[nodiscard]] Texture GetTexture() const;
	[[nodiscard]] RenderBuffer GetRenderBuffer() const;

	[[nodiscard]] bool IsComplete() const;

	[[nodiscard]] bool IsBound() const;

	void Bind() const;
	static void Unbind();

	[[nodiscard]] Color GetPixel(
		const V2_int& coordinate, ImageFormat format = Texture::default_format
	) const;

	void ForEachPixel(
		const std::function<void(V2_int, Color)>& func, ImageFormat format = Texture::default_format
	) const;

private:
	[[nodiscard]] static std::int32_t GetBoundId();

	void AttachTextureImpl(const Texture& texture);
	void AttachRenderBufferImpl(const RenderBuffer& render_buffer);
};

} // namespace ptgn