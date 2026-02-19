#pragma once

#include <optional>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/renderbuffer.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/texture.h"

namespace ptgn {

namespace impl {

namespace gl {

class Renderer;
class GLContext;

} // namespace gl

struct RenderTargetData {
	FramebufferId framebuffer_;
	std::optional<TextureId> color_;
	std::optional<RenderbufferId> depth_;
	// TODO: Consider using the cache values instead to prevent synchronization issues.
	V2_int size_;
	TextureFormat format_{ TextureFormat::RGBA8 };

	RenderTargetData() = default;

	RenderTargetData(
		const FramebufferId& framebuffer, const std::optional<TextureId>& color,
		const std::optional<RenderbufferId>& depth, V2_int size, TextureFormat format
	);

	void Resize(gl::GLContext& gl, V2_int new_size);

	void Bind(gl::GLContext& gl) const;

	void Clear(gl::GLContext& gl, Color color) const;

	bool operator==(const RenderTargetData&) const = default;
};

class RenderPass {
public:
	void Bind();

private:
	friend class impl::gl::Renderer;

	RenderTargetData source_;

	RenderTargetData ping_;
	RenderTargetData pong_;

	bool has_ping_{ false };
	bool has_pong_{ false };

	// "latest output" tracking
	bool has_written_once_{ false }; // false -> latest is source
	bool latest_is_ping_{ true };	 // valid only if has_written_once == true

	gl::Renderer* renderer_{ nullptr };
};

} // namespace impl

class RenderTarget : public impl::Resource<impl::RenderTargetData> {
public:
	using Base = impl::Resource<impl::RenderTargetData>;
	using Base::Base;

	V2_int GetSize() const;
	TextureFormat GetFormat() const;
	void Resize(V2_int new_size);

	void Bind();

	void Clear(Color color = color::Transparent);

private:
	friend class impl::gl::Renderer;

	RenderTarget() = default;
	RenderTarget(impl::gl::GLContext* gl, V2_int size, TextureFormat format);
};

} // namespace ptgn