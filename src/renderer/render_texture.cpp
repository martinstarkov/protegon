#include "renderer/render_texture.h"

#include "camera/camera.h"
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
#include "utility/handle.h"

namespace ptgn {

RenderTexture::RenderTexture(const V2_float& size, const Color& clear_color) :
	texture_{ nullptr,
			  size,
			  ImageFormat::RGB888,
			  TextureWrapping::ClampEdge,
			  TextureFilter::Nearest,
			  TextureFilter::Nearest,
			  false },
	clear_color_{ clear_color } {
	PTGN_ASSERT(texture_.IsValid(), "Failed to create render texture");
	frame_buffer_ = FrameBuffer{ texture_, RenderBuffer{ size }, clear_color_ };
	PTGN_ASSERT(frame_buffer_.IsValid(), "Failed to create frame buffer for render texture");
}

void RenderTexture::DrawAndUnbind(bool force_draw) const {
	game.draw.Flush();
	if (cleared_ && !force_draw) {
		// If nothing was flushed onto the render target, skip the draw and unbind. Prevents dual
		// drawing of the final target.
		return;
	}
	FrameBuffer::Unbind();
	game.draw.Shader(ScreenShader::Default, GetTexture(), BlendMode::Add);
	game.draw.FlushImpl(game.camera.GetWindow().GetViewProjection());
}

void RenderTexture::Clear() {
	frame_buffer_.Clear(clear_color_);
	cleared_ = true;
}

bool RenderTexture::IsValid() const {
	return frame_buffer_.IsValid();
}

bool RenderTexture::operator==(const RenderTexture& o) const {
	return frame_buffer_ == o.frame_buffer_;
}

bool RenderTexture::operator!=(const RenderTexture& o) const {
	return !(*this == o);
}

void RenderTexture::Bind() const {
	PTGN_ASSERT(frame_buffer_.IsValid());
	frame_buffer_.Bind();
}

Color RenderTexture::GetClearColor() const {
	return clear_color_;
}

void RenderTexture::SetClearColor(const Color& clear_color) {
	clear_color_ = clear_color;
}

FrameBuffer RenderTexture::GetFrameBuffer() const {
	return frame_buffer_;
}

Texture RenderTexture::GetTexture() const {
	return texture_;
}

} // namespace ptgn