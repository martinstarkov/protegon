#include "renderer/render_target.h"

#include <cstdint>
#include <functional>
#include <type_traits>
#include <vector>

#include "core/game.h"
#include "core/window.h"
#include "event/event_handler.h"
#include "event/events.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"
#include "renderer/gl_types.h"
#include "renderer/origin.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

RenderTargetInstance::RenderTargetInstance(const Color& clear_color, BlendMode blend_mode) :
	texture_{ Texture::WindowTexture{} },
	frame_buffer_{ texture_,
				   false /* do not rebind previous frame buffer because it will be cleared */ },
	blend_mode_{ blend_mode },
	clear_color_{ clear_color } {
	SubscribeToEvents();
	PTGN_ASSERT(V2_int{ camera_.GetSize() } == texture_.GetSize());
	PTGN_ASSERT(V2_int{ viewport_.size } == game.window.GetSize());
	PTGN_ASSERT(frame_buffer_.IsValid(), "Failed to create valid frame buffer for render target");
	Clear();
}

RenderTargetInstance::RenderTargetInstance(
	const V2_float& size, const Color& clear_color, BlendMode blend_mode
) :
	texture_{ size },
	frame_buffer_{
		texture_, false /* do not rebind previous frame buffer because it will be cleared */
	},
	blend_mode_{ blend_mode },
	clear_color_{ clear_color },
	viewport_{ {}, size, Origin::TopLeft } {
	camera_.CenterOnArea(size);
	PTGN_ASSERT(V2_int{ camera_.GetSize() } == texture_.GetSize());
	PTGN_ASSERT(viewport_.size == size);
	PTGN_ASSERT(frame_buffer_.IsValid(), "Failed to create valid frame buffer for render target");
	Clear();
}

RenderTargetInstance::~RenderTargetInstance() {
	UnsubscribeFromEvents();
}

void RenderTargetInstance::Bind() const {
	PTGN_ASSERT(frame_buffer_.IsValid(), "Cannot bind invalid or uninitialized frame buffer");
	frame_buffer_.Bind();
	PTGN_ASSERT(frame_buffer_.IsBound(), "Failed to bind render target frame buffer");
}

void RenderTargetInstance::SubscribeToEvents() {
	auto f{ std::function([this](const WindowResizedEvent& e) {
		viewport_ = { {}, e.size, Origin::TopLeft };
	}) };
	game.event.window.Subscribe(WindowEvent::Resized, this, f);
	std::invoke(f, WindowResizedEvent{ { game.window.GetSize() } });
}

void RenderTargetInstance::UnsubscribeFromEvents() const {
	game.event.window.Unsubscribe(this);
}

void RenderTargetInstance::Clear() const {
	PTGN_ASSERT(
		frame_buffer_.IsBound(), "Render target frame buffer must be bound before clearing"
	);
	GLRenderer::ClearToColor(clear_color_);
}

void RenderTargetInstance::DrawToScreen() const {
	FrameBuffer::Unbind();
	// Screen target replaces the screen frame buffer. Since the default frame buffer (id=0) is
	// transparent, premultiplied blend leads to destRGBA = srcRGBA blending (same as none). This,
	// however, results in less blend mode changes.
	GLRenderer::SetBlendMode(BlendMode::BlendPremultiplied);
	GLRenderer::SetViewport(viewport_.Min(), viewport_.size);
	const Shader& shader{ game.shader.Get(ScreenShader::Default) };
	shader.Bind();
	shader.SetUniform("u_ViewProjection", Camera{});
	texture_.DrawToBoundFrameBuffer(viewport_, {}, shader);
}

void RenderTargetInstance::Draw(
	const TextureInfo& texture_info, Shader shader, bool clear_after_draw
) const {
	GLRenderer::SetViewport(viewport_.Min(), viewport_.size);
	if (!shader.IsValid()) {
		shader = game.shader.Get(ScreenShader::Default);
	}
	shader.Bind();
	shader.SetUniform("u_ViewProjection", camera_);
	texture_.Draw(viewport_, texture_info, shader);

	if (clear_after_draw) {
		// Render target is cleared after drawing it to the current render target.
		Bind();
		Clear();
	}
}

const Rect& RenderTargetInstance::GetViewport() const {
	return viewport_;
}

void RenderTargetInstance::SetViewport(const Rect& viewport) {
	PTGN_ASSERT(
		viewport.size.x > 0 && viewport.size.y > 0,
		"Cannot set render target viewport with invalid size"
	);
	viewport_ = viewport;
}

Camera& RenderTargetInstance::GetCamera() {
	return camera_;
}

const Camera& RenderTargetInstance::GetCamera() const {
	return camera_;
}

void RenderTargetInstance::SetCamera(const Camera& camera) {
	camera_ = camera;
}

Color RenderTargetInstance::GetClearColor() const {
	return clear_color_;
}

void RenderTargetInstance::SetClearColor(const Color& clear_color) {
	clear_color_ = clear_color;
}

BlendMode RenderTargetInstance::GetBlendMode() const {
	return blend_mode_;
}

void RenderTargetInstance::SetBlendMode(BlendMode blend_mode) {
	blend_mode_ = blend_mode;
}

const Texture& RenderTargetInstance::GetTexture() const {
	return texture_;
}

V2_float RenderTargetInstance::TransformToTarget(const V2_float& screen_relative_coordinate) const {
	return ptgn::TransformToViewport(viewport_, camera_, screen_relative_coordinate);
}

V2_float RenderTargetInstance::TransformToScreen(const V2_float& target_relative_coordinate) const {
	return ptgn::TransformToScreen(viewport_, camera_, target_relative_coordinate);
}

V2_float RenderTargetInstance::ScaleToTarget(const V2_float& screen_relative_size) const {
	return ptgn::ScaleToViewport(viewport_, camera_, screen_relative_size);
}

V2_float RenderTargetInstance::ScaleToScreen(const V2_float& target_relative_size) const {
	return ptgn::ScaleToScreen(viewport_, camera_, target_relative_size);
}

Color RenderTargetInstance::GetPixel(const V2_int& coordinate) const {
	V2_int size{ texture_.GetSize() };
	PTGN_ASSERT(
		coordinate.x >= 0 && coordinate.x < size.x,
		"Cannot get pixel out of range of frame buffer texture"
	);
	PTGN_ASSERT(
		coordinate.y >= 0 && coordinate.y < size.y,
		"Cannot get pixel out of range of frame buffer texture"
	);
	auto formats{ impl::GetGLFormats(texture_.GetFormat()) };
	PTGN_ASSERT(
		formats.components_ >= 3,
		"Cannot retrieve pixel data of render target texture with less than 3 RGB components"
	);
	std::vector<std::uint8_t> v(static_cast<std::size_t>(formats.components_ * 1 * 1));
	int y{ size.y - 1 - coordinate.y };
	PTGN_ASSERT(y >= 0);
	Bind();
	GLCall(gl::glReadPixels(
		coordinate.x, y, 1, 1, formats.format_, static_cast<gl::GLenum>(impl::GLType::UnsignedByte),
		static_cast<void*>(v.data())
	));
	return Color{ v[0], v[1], v[2],
				  formats.components_ == 4 ? v[3] : static_cast<std::uint8_t>(255) };
}

void RenderTargetInstance::ForEachPixel(const std::function<void(V2_int, Color)>& func) const {
	V2_int size{ texture_.GetSize() };
	auto formats{ impl::GetGLFormats(texture_.GetFormat()) };
	PTGN_ASSERT(
		formats.components_ >= 3,
		"Cannot retrieve pixel data of render target texture with less than 3 RGB components"
	);

	std::vector<std::uint8_t> v(static_cast<std::size_t>(formats.components_ * size.x * size.y));
	Bind();
	GLCall(gl::glReadPixels(
		0, 0, size.x, size.y, formats.format_, static_cast<gl::GLenum>(impl::GLType::UnsignedByte),
		static_cast<void*>(v.data())
	));
	for (int j{ 0 }; j < size.y; j++) {
		// Ensure left-to-right and top-to-bottom iteration.
		int row{ (size.y - 1 - j) * size.x * formats.components_ };
		for (int i{ 0 }; i < size.x; i++) {
			int idx{ row + i * formats.components_ };
			PTGN_ASSERT(static_cast<std::size_t>(idx) < v.size());
			Color color{ v[static_cast<std::size_t>(idx)], v[static_cast<std::size_t>(idx + 1)],
						 v[static_cast<std::size_t>(idx + 2)],
						 formats.components_ == 4 ? v[static_cast<std::size_t>(idx + 3)]
												  : static_cast<std::uint8_t>(255) };
			std::invoke(func, V2_int{ i, j }, color);
		}
	}
}

} // namespace impl

RenderTarget::RenderTarget(const Color& clear_color, BlendMode blend_mode) {
	Create(clear_color, blend_mode);
}

RenderTarget::RenderTarget(const V2_float& size, const Color& clear_color, BlendMode blend_mode) {
	Create(size, clear_color, blend_mode);
}

V2_float RenderTarget::TransformToTarget(const V2_float& screen_relative_coordinate) const {
	return Get().TransformToTarget(screen_relative_coordinate);
}

V2_float RenderTarget::TransformToScreen(const V2_float& target_relative_coordinate) const {
	return Get().TransformToScreen(target_relative_coordinate);
}

V2_float RenderTarget::ScaleToTarget(const V2_float& screen_relative_size) const {
	return Get().ScaleToTarget(screen_relative_size);
}

V2_float RenderTarget::ScaleToScreen(const V2_float& target_relative_size) const {
	return Get().ScaleToScreen(target_relative_size);
}

void RenderTarget::Clear() const {
	auto& i{ Get() };
	i.Bind();
	i.Clear();
}

Color RenderTarget::GetClearColor() const {
	return Get().GetClearColor();
}

void RenderTarget::SetClearColor(const Color& clear_color) {
	Get().SetClearColor(clear_color);
}

const Camera& RenderTarget::GetCamera() const {
	return Get().GetCamera();
}

Camera& RenderTarget::GetCamera() {
	return Get().GetCamera();
}

void RenderTarget::SetCamera(const Camera& camera) {
	return Get().SetCamera(camera);
}

void RenderTarget::Draw(
	const TextureInfo& texture_info, const Shader& shader, bool clear_after_draw
) const {
	/*PTGN_ASSERT(
		game.renderer.GetRenderTarget() != *this,
		"Cannot draw a render target to itself. If you really want to create a new render target "
		"and draw to it"
	);*/
	Get().Draw(texture_info, shader, clear_after_draw);
}

const Texture& RenderTarget::GetTexture() const {
	return Get().GetTexture();
}

Color RenderTarget::GetPixel(const V2_int& coordinate) const {
	return Get().GetPixel(coordinate);
}

void RenderTarget::ForEachPixel(const std::function<void(V2_int, Color)>& func) const {
	Get().ForEachPixel(func);
}

} // namespace ptgn
