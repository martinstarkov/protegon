#include "rendering/resources/render_target.h"

#include <functional>
#include <memory>
#include <vector>

#include "common/assert.h"
#include "components/draw.h"
#include "components/drawable.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/window.h"
#include "events/event_handler.h"
#include "events/events.h"
#include "math/vector2.h"
#include "rendering/api/color.h"
#include "rendering/buffers/frame_buffer.h"
#include "rendering/gl/gl_renderer.h"
#include "rendering/render_data.h"
#include "rendering/resources/texture.h"
#include "scene/scene.h"

namespace ptgn {

namespace impl {

RenderTarget CreateRenderTarget(
	const Entity& entity, const Color& clear_color, TextureFormat texture_format
) {
	V2_int size{ game.window.GetSize() };
	RenderTarget render_target{ CreateRenderTarget(entity, size, clear_color, texture_format) };
	game.event.window.Subscribe(
		WindowEvent::Resized, entity, std::function([entity](const WindowResizedEvent& e) {
			RenderTarget{ entity }.GetTexture().Resize(e.size);
		})
	);
	return render_target;
}

RenderTarget CreateRenderTarget(
	const Entity& entity, const V2_float& size, const Color& clear_color,
	TextureFormat texture_format
) {
	RenderTarget render_target{ entity };
	render_target.SetPosition({});
	render_target.SetDraw<RenderTarget>();
	render_target.Add<TextureHandle>();
	render_target.Add<impl::DisplayList>();
	render_target.Show();
	// TODO: Add camera which resizes with size.
	render_target.Add<impl::ClearColor>(clear_color);
	// TODO: Move frame buffer object to a FrameBufferManager.
	const auto& frame_buffer{ render_target.Add<impl::FrameBuffer>(impl::Texture{
		nullptr, size, texture_format }) };
	PTGN_ASSERT(frame_buffer.IsValid(), "Failed to create valid frame buffer for render target");
	PTGN_ASSERT(frame_buffer.IsBound(), "Failed to bind frame buffer for render target");
	render_target.Clear();
	return render_target;
}

} // namespace impl

RenderTarget CreateRenderTarget(
	Scene& scene, const V2_float& size, const Color& clear_color, TextureFormat texture_format
) {
	return impl::CreateRenderTarget(scene.CreateEntity(), size, clear_color, texture_format);
}

RenderTarget CreateRenderTarget(
	Scene& scene, const Color& clear_color, TextureFormat texture_format
) {
	return impl::CreateRenderTarget(scene.CreateEntity(), clear_color, texture_format);
}

RenderTarget::RenderTarget(const Entity& entity) : Entity{ entity } {}

void RenderTarget::Draw(impl::RenderData& ctx, const Entity& entity) {
	impl::DrawTexture(ctx, entity, true);
}

V2_int RenderTarget::GetTextureSize() const {
	return impl::GetTextureSize(*this);
}

V2_int RenderTarget::GetSize() const {
	return impl::GetCroppedSize(*this);
}

V2_float RenderTarget::GetDisplaySize() const {
	return impl::GetDisplaySize(*this);
}

void RenderTarget::Bind() const {
	const auto& frame_buffer{ Get<impl::FrameBuffer>() };
	PTGN_ASSERT(frame_buffer.IsValid(), "Cannot bind invalid or uninitialized frame buffer");
	frame_buffer.Bind();
	PTGN_ASSERT(frame_buffer.IsBound(), "Failed to bind render target frame buffer");
}

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

void RenderTarget::ClearDisplayList() {
	PTGN_ASSERT(Has<impl::DisplayList>());
	auto& display_list{ Get<impl::DisplayList>().entities };
	for (Entity entity : display_list) {
		if (entity) {
			entity.Remove<RenderTarget>();
		}
	}
	display_list.clear();
}

void RenderTarget::AddToDisplayList(Entity& entity) {
	PTGN_ASSERT(entity, "Cannot add invalid entity to render target");
	PTGN_ASSERT(
		entity.Has<IDrawable>(), "Entity added to render target display list must be drawable"
	);
	PTGN_ASSERT(Has<impl::DisplayList>());
	auto& dl{ Get<impl::DisplayList>().entities };
	dl.emplace_back(entity);
	entity.Add<RenderTarget>(*this);
}

void RenderTarget::RemoveFromDisplayList(Entity& entity) {
	PTGN_ASSERT(entity, "Cannot remove invalid entity from render target");
	PTGN_ASSERT(
		entity.Has<IDrawable>(), "Entity remove from render target display list must be drawable"
	);
	entity.Remove<RenderTarget>();
	PTGN_ASSERT(Has<impl::DisplayList>());
	auto& dl{ Get<impl::DisplayList>().entities };
	dl.erase(std::remove(dl.begin(), dl.end(), entity), dl.end());
}

const std::vector<Entity>& RenderTarget::GetDisplayList() const {
	return Get<impl::DisplayList>().entities;
}

std::vector<Entity>& RenderTarget::GetDisplayList() {
	return Get<impl::DisplayList>().entities;
}

Color RenderTarget::GetClearColor() const {
	return GetOrDefault<impl::ClearColor>();
}

void RenderTarget::SetClearColor(const Color& clear_color) {
	Add<impl::ClearColor>(clear_color);
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