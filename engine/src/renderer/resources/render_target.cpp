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

void RenderTargetData::Bind(gl::GLContext& gl) const {
	auto _ = gl.Bind(framebuffer_);

	gl.SetViewport({ {}, size_ });
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

} // namespace impl

V2_int RenderTarget::GetSize() const {
	return resource_.size_;
}

TextureFormat RenderTarget::GetFormat() const {
	return resource_.format_;
}

void RenderTarget::Resize(V2_int new_size) {
	PTGN_ASSERT(IsValid());
	resource_.Resize(*renderer_->gl, new_size);
}

void RenderTarget::Clear(Color color) {
	PTGN_ASSERT(IsValid());
	resource_.Clear(*renderer_->gl, color);
}

void RenderTarget::Bind() {
	PTGN_ASSERT(IsValid());
	resource_.Bind(*renderer_->gl);
}

} // namespace ptgn

// void GameResizeScript::OnGameSizeChanged() {
//	auto game_size{ Application::resource_.render_.GetGameSize() };
//	RenderTarget{ entity }.Resize(game_size);
// }
//
// void DisplayResizeScript::OnDisplaySizeChanged() {
//	auto display_size{ Application::resource_.render_.GetDisplaySize() };
//	RenderTarget{ entity }.Resize(display_size);
// }
//
// } // namespace impl
//
// RenderTarget::RenderTarget(const Entity& entity) : Entity{ entity } {}
//
// void RenderTarget::Draw(const Entity& entity) {
//	Sprite sprite{ entity };
//
//	Camera camera{ RenderTarget{ entity }.GetCamera() };
//
//	V2_float texture_size;
//
//	if (camera) {
//		texture_size = camera.GetViewportSize();
//	}
//
//	if (texture_size.IsZero()) {
//		texture_size = sprite.GetSize();
//	}
//
//	Application::resource_.render_.DrawTexture(
//		sprite.GetTexture(), GetDrawTransform(entity), texture_size, GetDrawOrigin(entity),
//		GetTint(entity), GetDepth(entity), GetBlendMode(entity), entity.GetOrDefault<Camera>(),
//		entity.GetOrDefault<PreFX>(), entity.GetOrDefault<PostFX>(),
//		sprite.GetTextureCoordinates(true)
//	);
// }
//
// V2_int RenderTarget::GetTextureSize() const {
//	return impl::GetTextureSize(*this);
// }
//
// V2_int RenderTarget::GetSize() const {
//	return impl::GetCroppedSize(*this);
// }
//
// V2_float RenderTarget::GetDisplaySize() const {
//	return impl::GetDisplaySize(*this);
// }
//
// const Camera& RenderTarget::GetCamera() const {
//	PTGN_ASSERT(Has<GameObject<Camera>>());
//	return Get<GameObject<Camera>>();
// }
//
// Camera& RenderTarget::GetCamera() {
//	PTGN_ASSERT(Has<GameObject<Camera>>());
//	return Get<GameObject<Camera>>();
// }
//
// void RenderTarget::Bind() const {
//	const auto& frame_buffer{ Get<impl::FrameBuffer>() };
//	PTGN_ASSERT(frame_buffer.IsValid(), "Cannot bind invalid or uninitialized frame buffer");
//	frame_buffer.Bind();
//	PTGN_ASSERT(frame_buffer.IsBound(), "Failed to bind render target frame buffer");
// }
//
// void RenderTarget::Clear() const {
//	auto clear_color{ GetOrDefault<impl::ClearColor>() };
//	ClearToColor(clear_color);
// }
//
// void RenderTarget::ClearToColor(const Color& color) const {
//	PTGN_ASSERT(Has<impl::FrameBuffer>(), "Cannot clear render target with no frame buffer");
//	const auto& frame_buffer{ Get<impl::FrameBuffer>() };
//	frame_buffer.Bind();
//	PTGN_ASSERT(IsBound(), "Frame buffer must be bound before clearing");
//	impl::GLRenderer::ClearToColor(color);
// }
//
// void RenderTarget::ClearDisplayList() {
//	PTGN_ASSERT(Has<impl::DisplayList>());
//	auto& display_list{ Get<impl::DisplayList>().entities };
//	for (Entity entity : display_list) {
//		if (entity) {
//			entity.Remove<RenderTarget>();
//		}
//	}
//	display_list.clear();
// }
//
// void RenderTarget::AddToDisplayList(Entity entity) {
//	PTGN_ASSERT(entity, "Cannot add invalid entity to render target");
//	PTGN_ASSERT(HasDraw(entity), "Entity added to render target display list must be drawable");
//	// TODO: Consider allowing render targets to be rendered to other render targets.
//	PTGN_ASSERT(
//		!entity.Has<impl::FrameBuffer>(),
//		"Cannot add a render target to the display list of another render target. This is because "
//		"render order of targets is not enforced. Perhaps in the future."
//	);
//	PTGN_ASSERT(Has<impl::DisplayList>());
//	auto& dl{ Get<impl::DisplayList>().entities };
//	dl.emplace_back(entity);
//	entity.Add<RenderTarget>(*this);
// }
//
// void RenderTarget::RemoveFromDisplayList(Entity entity) {
//	PTGN_ASSERT(entity, "Cannot remove invalid entity from render target");
//	PTGN_ASSERT(HasDraw(entity), "Entity remove from render target display list must be drawable");
//	entity.Remove<RenderTarget>();
//	PTGN_ASSERT(Has<impl::DisplayList>());
//	auto& dl{ Get<impl::DisplayList>().entities };
//	std::erase(dl, entity);
// }
//
// const std::vector<Entity>& RenderTarget::GetDisplayList() const {
//	PTGN_ASSERT(Has<impl::DisplayList>());
//	return Get<impl::DisplayList>().entities;
// }
//
// std::vector<Entity>& RenderTarget::GetDisplayList() {
//	PTGN_ASSERT(Has<impl::DisplayList>());
//	return Get<impl::DisplayList>().entities;
// }
//
// Color RenderTarget::GetClearColor() const {
//	return GetOrDefault<impl::ClearColor>();
// }
//
// void RenderTarget::SetClearColor(const Color& clear_color) {
//	Add<impl::ClearColor>(clear_color);
// }
//
// const impl::Texture& RenderTarget::GetTexture() const {
//	return GetFrameBuffer().GetTexture();
// }
//
// const impl::FrameBuffer& RenderTarget::GetFrameBuffer() const {
//	return Get<impl::FrameBuffer>();
// }
//
// Color RenderTarget::GetPixel(const V2_int& coordinate, bool restore_bind_state) const {
//	return GetFrameBuffer().GetPixel(coordinate, restore_bind_state);
// }
//
// void RenderTarget::ForEachPixel(
//	const std::function<void(V2_int, Color)>& func, bool restore_bind_state
//) const {
//	return GetFrameBuffer().ForEachPixel(func, restore_bind_state);
// }
//
// void RenderTarget::Resize(const V2_int& size) {
//	if (auto camera{ TryGet<GameObject<Camera>>() }; camera && !camera->IsGameCamera()) {
//		Camera::Resize(*camera, size, true, true);
//	}
//	Get<impl::FrameBuffer>().Resize(size);
// }
//
// RenderTarget& SetDrawFilterImpl(RenderTarget& render_target, std::string_view filter_name) {
//	EntityAccess::Add<IDrawFilter>(render_target, filter_name);
//	return render_target;
// }
//
// bool RenderTarget::HasDrawFilter() const {
//	return Has<impl::IDrawFilter>();
// }
//
// RenderTarget& RenderTarget::RemoveDrawFilter() {
//	impl::EntityAccess::Remove<impl::IDrawFilter>(*this);
//	return *this;
// }