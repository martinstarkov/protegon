#include "renderer/render_texture.h"

#include "core/game.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/gl_renderer.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/surface.h"
#include "renderer/texture.h"
#include "utility/debug.h"

namespace ptgn {

RenderTexture::RenderTexture(const V2_float& size, const Color& clear_color, BlendMode blend_mode) :
	Texture{ nullptr,
			 size,
			 ImageFormat::RGB888,
			 TextureWrapping::ClampEdge,
			 TextureFilter::Nearest,
			 TextureFilter::Nearest,
			 false } {
	clear_color_ = clear_color;
	blend_mode_	 = blend_mode;
	PTGN_ASSERT(Texture::IsValid(), "Failed to create render texture");
	frame_buffer_ = FrameBuffer{ *this, RenderBuffer{ size } };
	PTGN_ASSERT(frame_buffer_.IsValid(), "Failed to create frame buffer for render texture");
	window_camera_.SetToWindow(true);
}

void RenderTexture::DrawAndUnbind() {
	FrameBuffer::Unbind();
	game.draw.Shader(ScreenShader::Default, blend_mode_);
	game.draw.FlushImpl(window_camera_.GetViewProjection());
}

void RenderTexture::Clear() const {
	PTGN_ASSERT(frame_buffer_.IsBound(), "Cannot clear unbound render texture");
	GLRenderer::ClearColor(clear_color_);
	GLRenderer::Clear();
}

void RenderTexture::Bind() const {
	PTGN_ASSERT(frame_buffer_.IsValid());
	frame_buffer_.Bind();
	Clear();
}

Color RenderTexture::GetClearColor() const {
	return clear_color_;
}

void RenderTexture::SetClearColor(const Color& clear_color) {
	clear_color_ = clear_color;
}

BlendMode RenderTexture::GetBlendMode() const {
	return blend_mode_;
}

void RenderTexture::SetBlendMode(BlendMode blend_mode) {
	blend_mode_ = blend_mode;
}

} // namespace ptgn