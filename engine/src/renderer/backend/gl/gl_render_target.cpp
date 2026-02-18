#include "renderer/backend/gl/gl_render_target.h"

#include <optional>

#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_framebuffer.h"
#include "renderer/backend/gl/gl_renderbuffer.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl::gl {

RenderTargets::RenderTargets(GLContext& gl) : gl_{ gl } {}

RenderTarget RenderTargets::CreateRenderTarget(V2_int size, TextureFormat format) const {
	const auto& desc = GetTextureFormatDesc(format);

	Texture color = gl_.textures.CreateTexture(
		nullptr, desc.pixel_format, desc.pixel_type, size, desc.internal_format
	);

	std::optional<Renderbuffer> depth;
	if (desc.has_depth || desc.has_stencil) {
		auto rb_format{ desc.has_stencil ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT };

		depth = gl_.renderbuffers.CreateRenderbuffer(size, rb_format);
	}

	Framebuffer fb = gl_.framebuffers.CreateFramebuffer(
		color, Attachment::Color0, depth,
		desc.has_stencil ? Attachment::DepthStencil : Attachment::Depth
	);

	return RenderTarget{ fb, color, depth, size, format };
}

void RenderTargets::ResizeRenderTarget(RenderTarget& render_target, V2_int new_size) const {
	if (render_target.size_ == new_size) {
		return;
	}

	gl_.framebuffers.ResizeFramebuffer(render_target.framebuffer_, new_size);

	render_target.size_ = new_size;
}

void RenderTargets::DestroyRenderTarget(RenderTarget& render_target) {
	if (render_target.color_.has_value()) {
		gl_.textures.DestroyTexture(*render_target.color_);
	}
	if (render_target.depth_.has_value()) {
		gl_.renderbuffers.DestroyRenderbuffer(*render_target.depth_);
	}
	gl_.framebuffers.DestroyFramebuffer(render_target.framebuffer_);
	render_target.size_	  = {};
	render_target.format_ = {};
}

void RenderTargets::ClearRenderTarget(const RenderTarget& render_target, Color color) {
	auto bind_guard = gl_.Bind(render_target.framebuffer_, true);
	gl_.SetViewport({ {}, render_target.size_ });
	gl_.framebuffers.ClearToColor(render_target.framebuffer_, color);
}

void RenderTargets::BindRenderTarget(const RenderTarget& render_target) {
	auto _ = gl_.Bind(render_target.framebuffer_);
	gl_.SetViewport({ {}, render_target.size_ });
}

} // namespace ptgn::impl::gl