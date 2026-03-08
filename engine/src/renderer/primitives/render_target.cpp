#include "renderer/primitives/render_target.h"

#include <memory>
#include <optional>
#include <utility>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_renderer.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/framebuffer.h"
#include "renderer/primitives/renderbuffer.h"
#include "renderer/primitives/resource.h"
#include "renderer/primitives/texture.h"
#include "renderer/primitives/viewport.h"

namespace ptgn {

namespace impl {

void RenderTargetData::Resize(gl::GLContext& gl, V2_int new_size) {
	if (size_ == new_size) {
		return;
	}

	gl.framebuffers.ResizeFramebuffer(framebuffer_, new_size);

	size_ = new_size;
}

void RenderTargetData::Clear(gl::GLContext& gl, Color color, bool set_viewport) const {
	auto bind_guard = gl.Bind(framebuffer_, true);

	std::optional<Viewport> viewport;
	if (set_viewport) {
		viewport = gl.GetViewport();

		gl.SetViewport({ {}, size_ });
	}

	gl.framebuffers.ClearToColor(framebuffer_, color);

	if (set_viewport && viewport.has_value()) {
		gl.SetViewport(*viewport);
	}
}

RenderTargetData::operator TextureId() const {
	PTGN_ASSERT(
		color_.has_value(), "Cannot convert render target with no color attachment to a texture id"
	);
	return *color_;
}

void RenderTargetData::Bind(gl::GLRenderer& renderer) const {
	renderer.SetFramebuffer(framebuffer_);
}

RenderTargetData::RenderTargetData(
	FramebufferId framebuffer, const std::optional<TextureId>& color,
	const std::optional<RenderbufferId>& depth, V2_int size, TextureFormat format
) :
	framebuffer_{ framebuffer },
	color_{ color },
	depth_{ depth },
	size_{ size },
	format_{ format } {}

void RenderPass::Bind() {
	// Bind the next write target (opposite of latest output; ping for first write)
	RenderTargetData write;

	if (!has_written_once_) {
		write = ping_;
	} else {
		if (!has_pong_ && latest_is_ping_) {
			PTGN_ASSERT(renderer_ != nullptr);
			pong_	  = renderer_->AcquirePooledTarget(source_.size_, source_.format_);
			has_pong_ = true;
		}
		write = latest_is_ping_ ? pong_ : ping_;
	}

	write.Bind(*renderer_);
}

V2_int RenderTargetObject::GetSize() const {
	return resource_.size_;
}

TextureFormat RenderTargetObject::GetFormat() const {
	return resource_.format_;
}

void RenderTargetObject::Resize(V2_int new_size) {
	PTGN_ASSERT(*this);
	resource_.Resize(*renderer_->gl, new_size);
}

void RenderTargetObject::Clear(Color color, bool set_viewport) {
	PTGN_ASSERT(*this);
	resource_.Clear(*renderer_->gl, color, set_viewport);
}

void RenderTargetObject::Bind() {
	PTGN_ASSERT(*this);
	resource_.Bind(*renderer_);
}

RenderTargetObject::operator TextureId() const {
	return static_cast<TextureId>(static_cast<RenderTargetData>(*this));
}

} // namespace impl

} // namespace ptgn