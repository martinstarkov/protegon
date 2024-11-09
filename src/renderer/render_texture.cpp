#include "renderer/render_texture.h"

#include <functional>

#include "camera/camera.h"
#include "core/game.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/surface.h"
#include "renderer/texture.h"
#include "utility/debug.h"
#include "utility/handle.h"

namespace ptgn {

namespace impl {

RenderTextureInstance::RenderTextureInstance(
	bool continuously_window_sized, const V2_float& size, const Color& clear_color,
	BlendMode blend_mode
) {
	Recreate(size, clear_color, blend_mode);

	camera_.SetToWindow(continuously_window_sized);

	if (!continuously_window_sized) {
		return;
	}

	game.event.window.Subscribe(
		WindowEvent::Resized, this, std::function([this](const WindowResizedEvent&) {
			Recreate(game.window.GetSize(), clear_color_, blend_mode_);
		})
	);
}

RenderTextureInstance::~RenderTextureInstance() {
	game.event.window.Unsubscribe(this);
}

void RenderTextureInstance::Recreate(
	const V2_float& size, const Color& clear_color, BlendMode blend_mode
) {
	clear_color_ = clear_color;
	blend_mode_	 = blend_mode;
	texture_	 = Texture{ nullptr,
						size,
						ImageFormat::RGB888,
						TextureWrapping::ClampEdge,
						TextureFilter::Nearest,
						TextureFilter::Nearest,
						false };
	PTGN_ASSERT(texture_.IsValid(), "Failed to create render texture");
	frame_buffer_ = FrameBuffer{ texture_, RenderBuffer{ size }, clear_color_ };
	PTGN_ASSERT(frame_buffer_.IsValid(), "Failed to create frame buffer for render texture");
}

} // namespace impl

RenderTexture::RenderTexture(
	bool continuously_window_sized, const Color& clear_color, BlendMode blend_mode
) {
	Create(continuously_window_sized, game.window.GetSize(), clear_color, blend_mode);
}

RenderTexture::RenderTexture(const V2_float& size, const Color& clear_color, BlendMode blend_mode) {
	Create(false, size, clear_color, blend_mode);
}

void RenderTexture::DrawAndUnbind(bool force_draw) const {
	game.draw.Flush();
	auto& i{ Get() };
	if (i.cleared_ && !force_draw || i.opacity_ == 0) {
		// If nothing was flushed onto the render target, skip the draw and unbind. Prevents dual
		// drawing of the final target. Or render texture is set as completely transparent.
		return;
	}
	FrameBuffer::Unbind();
	if (i.opacity_ == 255) {
		game.draw.Shader(ScreenShader::Default, GetTexture(), i.blend_mode_);
	} else {
		auto opacity_shader{ game.shader.Get(ScreenShader::Opacity) };
		opacity_shader.Bind();
		float normalized_opacity{ i.opacity_ / 255.0f };
		opacity_shader.SetUniform("u_Opacity", normalized_opacity);
		game.draw.Shader(opacity_shader, GetTexture(), {}, {}, Origin::TopLeft, i.blend_mode_);
	}
	game.draw.FlushImpl(i.camera_.GetViewProjection());
}

void RenderTexture::Clear() {
	auto& i{ Get() };
	i.frame_buffer_.Clear(i.clear_color_);
	i.cleared_ = true;
}

V2_int RenderTexture::GetSize() const {
	return Get().texture_.GetSize();
}

void RenderTexture::Bind() const {
	auto& i{ Get() };
	PTGN_ASSERT(i.frame_buffer_.IsValid());
	i.frame_buffer_.Bind();
}

OrthographicCamera RenderTexture::GetCamera() {
	return Get().camera_;
}

void RenderTexture::SetCleared(bool cleared) {
	Get().cleared_ = cleared;
}

Color RenderTexture::GetClearColor() const {
	return Get().clear_color_;
}

void RenderTexture::SetClearColor(const Color& clear_color) {
	Get().clear_color_ = clear_color;
}

std::uint8_t RenderTexture::GetOpacity() const {
	return Get().opacity_;
}

void RenderTexture::SetOpacity(std::uint8_t opacity) {
	PTGN_ASSERT(opacity >= 0, "Cannot set negative opacity");
	Get().opacity_ = opacity;
}

BlendMode RenderTexture::GetBlendMode() const {
	return Get().blend_mode_;
}

void RenderTexture::SetBlendMode(BlendMode blend_mode) {
	Get().blend_mode_ = blend_mode;
}

FrameBuffer RenderTexture::GetFrameBuffer() const {
	return Get().frame_buffer_;
}

Texture RenderTexture::GetTexture() const {
	return Get().texture_;
}

} // namespace ptgn