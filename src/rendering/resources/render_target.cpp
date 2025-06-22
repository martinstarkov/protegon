#include "rendering/resources/render_target.h"

#include "common/assert.h"
#include "components/draw.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "rendering/batching/render_data.h"
#include "rendering/buffers/frame_buffer.h"
#include "rendering/gl/gl_renderer.h"
#include "rendering/renderer.h"
#include "rendering/resources/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"

namespace ptgn {

namespace impl {

RenderTarget CreateRenderTarget(
	const Entity& entity, const Camera& camera, const V2_float& size, const Color& clear_color,
	TextureFormat format
) {
	RenderTarget render_target{ entity };
	render_target.SetDraw<RenderTarget>();
	render_target.Add<TextureHandle>();
	render_target.Add<impl::RenderTargetEntities>();
	render_target.Show();
	render_target.Add<Camera>(camera);
	render_target.Add<impl::ClearColor>(clear_color);
	// TODO: Move frame buffer object to a FrameBufferManager.
	auto& frame_buffer{ render_target.Add<impl::FrameBuffer>(impl::Texture{ nullptr, size, format }
	) };
	PTGN_ASSERT(frame_buffer.IsValid(), "Failed to create valid frame buffer for render target");
	PTGN_ASSERT(frame_buffer.IsBound(), "Failed to bind frame buffer for render target");
	render_target.Clear();
	return render_target;
}

} // namespace impl

RenderTarget CreateRenderTarget(
	Scene& scene, const V2_float& size, const Color& clear_color, TextureFormat texture_format
) {
	return impl::CreateRenderTarget(
		scene.CreateEntity(), CreateCamera(scene), size, clear_color, texture_format
	);
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
	RenderTarget rt{ entity };

	const auto& texture{ rt.GetTexture() };

	PTGN_ASSERT(texture.IsValid(), "Cannot draw a render target with an invalid texture");

	auto depth{ entity.GetDepth() };
	auto blend_mode{ entity.GetBlendMode() };
	auto tint{ entity.GetTint().Normalized() };

	Sprite sprite{ entity };
	auto coords{ sprite.GetTextureCoordinates(true) };

	// TODO: Add custom sizing for render targets.
	// TODO: Fix.
	/*ctx.AddTexturedQuad(
		{}, {}, Origin::Center, coords, texture, depth, Camera{}, blend_mode, tint, false
	);*/
}

void RenderTarget::Draw(const Entity& entity) const {
	PTGN_ASSERT(entity, "Cannot draw invalid entity to render target");

	const auto& frame_buffer{ Get<impl::FrameBuffer>() };
	frame_buffer.Bind();
	PTGN_ASSERT(frame_buffer.IsBound(), "Cannot draw to render target unless it is first bound");

	// TODO: Fix.
	/*auto& rd{ game.renderer.GetRenderData() };

	const auto& camera{ GetCamera() };
	rd.fallback_camera = camera;
	rd.UseCamera(camera);

	rd.AddToBatch(entity, false);

	rd.Flush(frame_buffer);*/
}

const Camera& RenderTarget::GetCamera() const {
	PTGN_ASSERT(Has<Camera>(), "Render target must have a camera attached to it");
	return Get<Camera>();
}

Camera& RenderTarget::GetCamera() {
	PTGN_ASSERT(Has<Camera>(), "Render target must have a camera attached to it");
	return Get<Camera>();
}

void RenderTarget::Bind() const {
	const auto& frame_buffer{ Get<impl::FrameBuffer>() };
	PTGN_ASSERT(frame_buffer.IsValid(), "Cannot bind invalid or uninitialized frame buffer");
	frame_buffer.Bind();
	PTGN_ASSERT(frame_buffer.IsBound(), "Failed to bind render target frame buffer");
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
	PTGN_ASSERT(Has<impl::FrameBuffer>(), "Cannot clear render target with no frame buffer");
	const auto& frame_buffer{ Get<impl::FrameBuffer>() };
	frame_buffer.Bind();
	PTGN_ASSERT(frame_buffer.IsBound(), "Render target frame buffer must be bound before clearing");
	auto clear_color{ GetOrDefault<impl::ClearColor>() };
	impl::GLRenderer::ClearToColor(clear_color);
}

void RenderTarget::ClearToColor(const Color& color) const {
	PTGN_ASSERT(Has<impl::FrameBuffer>(), "Cannot clear render target with no frame buffer");
	const auto& frame_buffer{ Get<impl::FrameBuffer>() };
	frame_buffer.Bind();
	PTGN_ASSERT(frame_buffer.IsBound(), "Render target frame buffer must be bound before clearing");
	impl::GLRenderer::ClearToColor(color);
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

void RenderTarget::ClearEntities() {
	PTGN_ASSERT(Has<impl::RenderTargetEntities>());
	auto& entities{ Get<impl::RenderTargetEntities>().entities };
	for (Entity entity : entities) {
		if (entity) {
			entity.Remove<RenderTarget>();
		}
	}
	entities.clear();
}

void RenderTarget::AddEntity(Entity& entity) {
	PTGN_ASSERT(entity, "Cannot add invalid entity to render target");
	PTGN_ASSERT(Has<impl::RenderTargetEntities>());
	Get<impl::RenderTargetEntities>().entities.emplace(entity);
	entity.Add<RenderTarget>(*this);
}

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