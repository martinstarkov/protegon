#include "renderer/render_target.h"

#include <utility>

#include "core/game.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/gl_renderer.h"
#include "renderer/renderer.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "utility/assert.h"

namespace ptgn {

/*
RenderTarget::RenderTarget(const Color& clear_color) :
	texture_{ Texture::WindowTexture{} },
	frame_buffer_{ texture_,
				   false },
	blend_mode_{ blend_mode },
	clear_color_{ clear_color } {
	SubscribeToEvents();
	PTGN_ASSERT(V2_int{ camera_.GetSize() } == texture_.GetSize());
	PTGN_ASSERT(V2_int{ viewport_.size } == game.window.GetSize());
	PTGN_ASSERT(frame_buffer_.IsValid(), "Failed to create valid frame buffer for render target");
	Clear();
}
*/

RenderTarget::RenderTarget(ecs::Manager& manager, const V2_float& size, const Color& clear_color) :
	RenderTarget{ size, clear_color } {
	camera = CreateCamera(manager);
}

RenderTarget::RenderTarget(const V2_float& size, const Color& clear_color) :
	frame_buffer_{ impl::Texture(nullptr, size) }, clear_color_{ clear_color } {
	PTGN_ASSERT(frame_buffer_.IsValid(), "Failed to create valid frame buffer for render target");
	PTGN_ASSERT(frame_buffer_.IsBound(), "Failed to bind frame buffer for render target");
	Clear();
}

RenderTarget::RenderTarget(RenderTarget&& other) noexcept :
	camera{ std::exchange(other.camera, {}) },
	frame_buffer_{ std::exchange(other.frame_buffer_, {}) },
	clear_color_{ std::exchange(other.clear_color_, {}) } {
	// TODO: Add window subscribe stuff here.
}

RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept {
	if (this != &other) {
		// TODO: Add window subscribe stuff here.
		camera		  = std::exchange(other.camera, {});
		frame_buffer_ = std::exchange(other.frame_buffer_, {});
		clear_color_  = std::exchange(other.clear_color_, {});
	}
	return *this;
}

RenderTarget::~RenderTarget() {
	// TODO: Add window subscribe stuff here.
	// UnsubscribeFromEvents();
}

void RenderTarget::Draw(ecs::Entity e) const {
	PTGN_ASSERT(frame_buffer_.IsBound(), "Cannot draw to render target unless it is first bound");
	PTGN_ASSERT(
		camera != Camera{}, "Cannot draw to render target with invalid or uninitialized camera"
	);
	game.renderer.GetRenderData().Render(frame_buffer_, camera, e, false);
}

void RenderTarget::Bind() const {
	PTGN_ASSERT(frame_buffer_.IsValid(), "Cannot bind invalid or uninitialized frame buffer");
	frame_buffer_.Bind();
	PTGN_ASSERT(frame_buffer_.IsBound(), "Failed to bind render target frame buffer");
}

/*
// TODO: Add window subscribe stuff here.
void RenderTarget::SubscribeToEvents() {
	auto f{ std::function([this](const WindowResizedEvent& e) {
		viewport_ = { {}, e.size, Origin::TopLeft };
	}) };
	game.event.window.Subscribe(WindowEvent::Resized, this, f);
	std::invoke(f, WindowResizedEvent{ { game.window.GetSize() } });
}

void RenderTarget::UnsubscribeFromEvents() const {
	game.event.window.Unsubscribe(this);
}
*/

void RenderTarget::Clear() const {
	PTGN_ASSERT(
		frame_buffer_.IsBound(), "Render target frame buffer must be bound before clearing"
	);
	impl::GLRenderer::ClearToColor(clear_color_);
}

void RenderTarget::SetTint(const Color& color) {
	tint_color_ = color;
}

Color RenderTarget::GetTint() const {
	return tint_color_;
}

/*
void RenderTarget::DrawToScreen() const {
	FrameBuffer::Unbind();
	// Screen target replaces the screen frame buffer. Since the default frame buffer (id=0) is
	// transparent, premultiplied blend leads to destRGBA = srcRGBA blending (same as none). This,
	// however, results in less blend mode changes.
	GLRenderer::SetBlendMode(BlendMode::BlendPremultiplied);
	GLRenderer::SetViewport(viewport_.Min(), viewport_.size);
	const Shader& shader{ game.shader.Get(ScreenShader::Default) };
	shader.Bind();
	shader.SetUniform("u_ViewProjection", ...);
	texture_.DrawToBoundFrameBuffer(viewport_, {}, shader);
}

void RenderTarget::Draw(const TextureInfo& texture_info, Shader shader, bool clear_after_draw)
	const {
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
*/

Color RenderTarget::GetClearColor() const {
	return clear_color_;
}

void RenderTarget::SetClearColor(const Color& clear_color) {
	clear_color_ = clear_color;
}

const impl::Texture& RenderTarget::GetTexture() const {
	return frame_buffer_.GetTexture();
}

impl::Texture& RenderTarget::GetTexture() {
	return frame_buffer_.GetTexture();
}

const impl::FrameBuffer& RenderTarget::GetFrameBuffer() const {
	return frame_buffer_;
}

} // namespace ptgn