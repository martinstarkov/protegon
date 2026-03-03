#pragma once

#include <optional>

#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/framebuffer.h"
#include "renderer/primitives/renderbuffer.h"
#include "renderer/primitives/resource.h"
#include "renderer/primitives/texture.h"

namespace ptgn {

namespace impl {

namespace gl {

class GLRenderer;
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
		FramebufferId framebuffer, const std::optional<TextureId>& color,
		const std::optional<RenderbufferId>& depth, V2_int size, TextureFormat format
	);

	void Resize(gl::GLContext& gl, V2_int new_size);

	void Bind(gl::GLRenderer& renderer) const;

	void Clear(gl::GLContext& gl, Color color) const;

	bool operator==(const RenderTargetData&) const = default;

	operator TextureId() const;
};

class RenderPass {
public:
	void Bind();

private:
	friend class impl::gl::GLRenderer;

	RenderTargetData source_;

	RenderTargetData ping_;
	RenderTargetData pong_;

	bool has_ping_{ false };
	bool has_pong_{ false };

	// "latest output" tracking
	bool has_written_once_{ false }; // false -> latest is source
	bool latest_is_ping_{ true };	 // valid only if has_written_once == true

	gl::GLRenderer* renderer_{ nullptr };
};

class RenderTargetObject : public Resource<RenderTargetData> {
public:
	using Base = Resource<RenderTargetData>;
	using Base::Base;

	V2_int GetSize() const;
	TextureFormat GetFormat() const;
	void Resize(V2_int new_size);

	void Bind();

	void Clear(Color color = color::Transparent);

	operator TextureId() const;

private:
	friend class gl::GLRenderer;

	RenderTargetObject() = default;
	RenderTargetObject(gl::GLContext* gl, V2_int size, TextureFormat format);
};

} // namespace impl

} // namespace ptgn