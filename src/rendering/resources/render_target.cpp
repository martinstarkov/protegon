#include "rendering/resources/render_target.h"

#include "common/assert.h"
#include "components/draw.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/batching/render_data.h"
#include "rendering/buffers/frame_buffer.h"
#include "rendering/gl/gl_renderer.h"
#include "rendering/renderer.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"

namespace ptgn {

RenderTarget CreateRenderTarget(Manager& manager, const V2_float& size, const Color& clear_color) {
	RenderTarget render_target{ manager.CreateEntity() };
	render_target.SetDraw<RenderTarget>();
	render_target.Add<TextureHandle>();
	render_target.Show();
	render_target.Add<Camera>(CreateCamera(manager));
	render_target.Add<impl::ClearColor>(clear_color);
	// TODO: Move frame buffer object to a FrameBufferManager.
	auto& fb = render_target.Add<impl::FrameBuffer>(impl::Texture{ nullptr, size });
	PTGN_ASSERT(fb.IsValid(), "Failed to create valid frame buffer for render target");
	PTGN_ASSERT(fb.IsBound(), "Failed to bind frame buffer for render target");
	render_target.Clear();
	return render_target;
}

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

RenderTarget::RenderTarget(const Entity& entity) : Entity{ entity } {}

void RenderTarget::Draw(impl::RenderData& ctx, const Entity& entity) {
	Sprite::Draw(ctx, entity);
}

void RenderTarget::Draw(const Entity& e) const {
	const auto& fb	   = Get<impl::FrameBuffer>();
	const auto& camera = Get<Camera>();
	fb.Bind();
	PTGN_ASSERT(fb.IsBound(), "Cannot draw to render target unless it is first bound");
	PTGN_ASSERT(
		camera != Camera{}, "Cannot draw to render target with invalid or uninitialized camera"
	);
	game.renderer.GetRenderData().Render(fb, camera, e, false);
}

void RenderTarget::Bind() const {
	const auto& fb = Get<impl::FrameBuffer>();
	PTGN_ASSERT(fb.IsValid(), "Cannot bind invalid or uninitialized frame buffer");
	fb.Bind();
	PTGN_ASSERT(fb.IsBound(), "Failed to bind render target frame buffer");
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
	const auto& fb = Get<impl::FrameBuffer>();
	fb.Bind();
	PTGN_ASSERT(fb.IsBound(), "Render target frame buffer must be bound before clearing");
	const auto& clear_color = Get<impl::ClearColor>();
	impl::GLRenderer::ClearToColor(clear_color);
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
	return Has<impl::ClearColor>() ? Get<impl::ClearColor>() : impl::ClearColor{};
}

void RenderTarget::SetClearColor(const Color& clear_color) {
	if (Has<impl::ClearColor>()) {
		Get<impl::ClearColor>() = clear_color;
	} else {
		Add<impl::ClearColor>(clear_color);
	}
}

const impl::Texture& RenderTarget::GetTexture() const {
	return Get<impl::FrameBuffer>().GetTexture();
}

impl::Texture& RenderTarget::GetTexture() {
	return Get<impl::FrameBuffer>().GetTexture();
}

const impl::FrameBuffer& RenderTarget::GetFrameBuffer() const {
	return Get<impl::FrameBuffer>();
}

} // namespace ptgn