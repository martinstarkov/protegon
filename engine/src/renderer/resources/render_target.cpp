#include "renderer/resources/render_target.h"

#include <memory>
#include <optional>
#include <utility>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_renderer.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/renderbuffer.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/texture.h"

namespace ptgn {

namespace impl {

void RenderTargetData::Resize(gl::GLContext& gl, V2_int new_size) {
	if (size_ == new_size) {
		return;
	}

	gl.framebuffers.ResizeFramebuffer(framebuffer_, new_size);

	size_ = new_size;
}

void RenderTargetData::Clear(gl::GLContext& gl, Color color) const {
	auto bind_guard = gl.Bind(framebuffer_, true);

	gl.SetViewport({ {}, size_ });

	gl.framebuffers.ClearToColor(framebuffer_, color);
}

RenderTargetData::operator TextureId() const {
	PTGN_ASSERT(
		color_.has_value(), "Cannot convert render target with no color attachment to a texture id"
	);
	return *color_;
}

void RenderTargetData::Bind(gl::GLContext& gl) const {
	auto _ = gl.Bind(framebuffer_);
}

RenderTargetData::RenderTargetData(
	const FramebufferId& framebuffer, const std::optional<TextureId>& color,
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

	write.Bind(*renderer_->gl);
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

void RenderTargetObject::Clear(Color color) {
	PTGN_ASSERT(*this);
	resource_.Clear(*renderer_->gl, color);
}

void RenderTargetObject::Bind() {
	PTGN_ASSERT(*this);
	resource_.Bind(*renderer_->gl);
}

RenderTargetObject::operator TextureId() const {
	return static_cast<TextureId>(static_cast<RenderTargetData>(*this));
}

} // namespace impl

} // namespace ptgn